#define _GNU_SOURCE
#include "url.h"
#include "quote.h"
#include "siteconf.h"
#include "buffer_loader.h"
#include "form.h"
#include "istream.h"
#include "display.h"
#include "buffer.h"
#include "http.h"
#include "indep.h"
#include "mysignal.h"
#include "local.h"
#include "proxy.h"
#include "rc.h"
#include "ui.h"
#include "cookie.h"
#include "ssl_util.h"
#include "etc.h"
#include "tty.h"
#include <openssl/ssl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>
#include <setjmp.h>
#include <errno.h>

#include <sys/stat.h>

#include "Str.h"
#include "myctype.h"
#include "regex.h"

int DNS_order = DNS_ORDER_UNSPEC;
char ArgvIsURL = true;
char LocalhostOnly = false;
char* document_root = NULL;
int retryAsHttp = true;
char* index_file = NULL;
int DecodeURL = false;

/* see rc.c, "dns_order" and dnsorders[] */
int ai_family_order_table[7][3] = {
    { PF_UNSPEC, PF_UNSPEC, PF_UNSPEC }, /* 0:unspec */
    { PF_INET, PF_INET6, PF_UNSPEC }, /* 1:inet inet6 */
    { PF_INET6, PF_INET, PF_UNSPEC }, /* 2:inet6 inet */
    { PF_UNSPEC, PF_UNSPEC, PF_UNSPEC }, /* 3: --- */
    { PF_INET, PF_UNSPEC, PF_UNSPEC }, /* 4:inet */
    { PF_UNSPEC, PF_UNSPEC, PF_UNSPEC }, /* 5: --- */
    { PF_INET6, PF_UNSPEC, PF_UNSPEC }, /* 6:inet6 */
};

static sigjmp_buf AbortLoading;

/* #define HTTP_DEFAULT_FILE    "/index.html" */

#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

#ifdef SOCK_DEBUG
#include <stdarg.h>

static void
sock_log(char* message, ...)
{
    FILE* f = fopen("zzzsocklog", "a");
    va_list va;

    if (f == NULL)
        return;
    va_start(va, message);
    vfprintf(f, message, va);
    fclose(f);
}

#endif

static char*
DefaultFile(int scheme)
{
    switch (scheme) {
    case SCM_HTTP:
    case SCM_HTTPS:
        return allocStr(HTTP_DEFAULT_FILE, -1);
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
    case SCM_FTP:
    case SCM_FTPDIR:
        return allocStr("/", -1);
    }
    return NULL;
}

static MySignalHandler
KeyAbort(int _dummy)
{
    siglongjmp(AbortLoading, 1);
}

ParsedURL*
baseURL(Buffer* buf)
{
    if (buf->bufferprop & BP_NO_URL) {
        /* no URL is defined for the buffer */
        return NULL;
    }
    if (buf->baseURL != NULL) {
        /* <BASE> tag is defined in the document */
        return buf->baseURL;
    } else if (IS_EMPTY_PARSED_URL(&buf->currentURL))
        return NULL;
    else
        return &buf->currentURL;
}

int openSocket(const char* hostname,
    const char* remoteport_name, unsigned short remoteport_num)
{
    volatile int sock = -1;
    int* af;
    struct addrinfo hints, *res0, *res;
    int error;
    char* hname;
    MySignalHandler (*volatile prevtrap)(int _dummy) = NULL;

    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, Sprintf("Opening socket...")->ptr);
    // refresh(ttyWriter());

    if (sigsetjmp(AbortLoading, 1) != 0) {
#ifdef SOCK_DEBUG
        sock_log("openSocket() failed. reason: user abort\n");
#endif
        if (sock >= 0)
            close(sock);
        goto error;
    }
    TRAP_ON;
    if (hostname == NULL) {
#ifdef SOCK_DEBUG
        sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n",
            hostname);
#endif
        goto error;
    }

    /* rfc2732 compliance */
    hname = (char*)hostname;
    if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
        hname = allocStr(hostname + 1, -1);
        hname[strlen(hname) - 1] = '\0';
        if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
            goto error;
    }
    for (af = ai_family_order_table[DNS_order];; af++) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = *af;
        hints.ai_socktype = SOCK_STREAM;
        if (remoteport_num != 0) {
            Str portbuf = Sprintf("%d", remoteport_num);
            error = getaddrinfo(hname, portbuf->ptr, &hints, &res0);
        } else {
            error = -1;
        }
        if (error && remoteport_name && remoteport_name[0] != '\0') {
            /* try default port */
            error = getaddrinfo(hname, remoteport_name, &hints, &res0);
        }
        if (error) {
            if (*af == PF_UNSPEC) {
                goto error;
            }
            /* try next ai family */
            continue;
        }
        sock = -1;
        for (res = res0; res; res = res->ai_next) {
            sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sock < 0) {
                continue;
            }
            if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
                close(sock);
                sock = -1;
                continue;
            }
            break;
        }
        if (sock < 0) {
            freeaddrinfo(res0);
            if (*af == PF_UNSPEC) {
                goto error;
            }
            /* try next ai family */
            continue;
        }
        freeaddrinfo(res0);
        break;
    }

    TRAP_OFF;
    return sock;
error:
    TRAP_OFF;
    return -1;
}

#define COPYPATH_SPC_ALLOW 0
#define COPYPATH_SPC_IGNORE 1
#define COPYPATH_SPC_REPLACE 2
#define COPYPATH_SPC_MASK 3
#define COPYPATH_LOWERCASE 4

static char*
copyPath(const char* orgpath, int length, int option)
{
    Str tmp = Strnew();
    char ch;
    while ((ch = *orgpath) != 0 && length != 0) {
        if (option & COPYPATH_LOWERCASE)
            ch = TOLOWER(ch);
        if (IS_SPACE(ch)) {
            switch (option & COPYPATH_SPC_MASK) {
            case COPYPATH_SPC_ALLOW:
                Strcat_char(tmp, ch);
                break;
            case COPYPATH_SPC_IGNORE:
                /* do nothing */
                break;
            case COPYPATH_SPC_REPLACE:
                Strcat_charp(tmp, "%20");
                break;
            }
        } else
            Strcat_char(tmp, ch);
        orgpath++;
        length--;
    }
    return tmp->ptr;
}

void parseURL(const char* _url, ParsedURL* p_url, ParsedURL* current)
{
    // quote 0x01-0x20, 0x7F-0xFF
    const char* url = url_quote(_url);

    const char* p = url;
    copyParsedURL(p_url, NULL);
    p_url->scheme = SCM_MISSING;

    // RFC1808: Relative Uniform Resource Locators
    // 4.  Resolving Relative URLs
    if (*url == '\0' || *url == '#') {
        if (current)
            copyParsedURL(p_url, current);
        goto do_label;
    }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (IS_ALPHA(*p) && (p[1] == ':' || p[1] == '|')) {
        p_url->scheme = SCM_LOCAL;
        goto analyze_file;
    }
#endif /* SUPPORT_DOS_DRIVE_PREFIX */
    /* search for scheme */
    p_url->scheme = parseUrlScheme(&p);
    if (p_url->scheme == SCM_MISSING) {
        /* scheme part is not found in the url. This means either
         * (a) the url is relative to the current or (b) the url
         * denotes a filename (therefore the scheme is SCM_LOCAL).
         */
        if (current) {
            switch (current->scheme) {
            case SCM_LOCAL:
            case SCM_LOCAL_CGI:
                p_url->scheme = SCM_LOCAL;
                break;
            case SCM_FTP:
            case SCM_FTPDIR:
                p_url->scheme = SCM_FTP;
                break;
            default:
                p_url->scheme = current->scheme;
                break;
            }
        } else
            p_url->scheme = SCM_LOCAL;
        p = url;
        if (!strncmp(p, "//", 2)) {
            /* URL begins with // */
            /* it means that 'scheme:' is abbreviated */
            p += 2;
            goto analyze_url;
        }
        /* the url doesn't begin with '//' */
        goto analyze_file;
    }
    /* scheme part has been found */
    if (p_url->scheme == SCM_UNKNOWN) {
        p_url->file = allocStr(url, -1);
        return;
    }
    /* get host and port */
    if (p[0] != '/' || p[1] != '/') { /* scheme:foo or scheme:/foo */
        p_url->host = NULL;
        p_url->port = getSchemeInfo(p_url->scheme).port;
        goto analyze_file;
    }
    /* after here, p begins with // */
    if (p_url->scheme == SCM_LOCAL) { /* file://foo           */
        if (p[2] == '/' || p[2] == '~'
        /* <A HREF="file:///foo">file:///foo</A>  or <A HREF="file://~user">file://~user</A> */
#ifdef SUPPORT_DOS_DRIVE_PREFIX
            || (IS_ALPHA(p[2]) && (p[3] == ':' || p[3] == '|'))
        /* <A HREF="file://DRIVE/foo">file://DRIVE/foo</A> */
#endif /* SUPPORT_DOS_DRIVE_PREFIX */
        ) {
            p += 2;
            goto analyze_file;
        }
    }
    p += 2; /* scheme://foo         */
    /*          ^p is here  */
analyze_url:
    const char* q = p;

    if (*q == '[') { /* rfc2732,rfc2373 compliance */
        p++;
        while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
            p++;
        if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == NULL))
            p = q;
    }

    while (*p && strchr(":/@?#", *p) == NULL)
        p++;

    Str tmp;
    switch (*p) {
    case ':': {
        /* scheme://user:pass@host or
         * scheme://host:port
         */
        const char* qq = q;
        q = ++p;
        while (*p && strchr("@/?#", *p) == NULL)
            p++;
        if (*p == '@') {
            /* scheme://user:pass@...       */
            p_url->user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
            p_url->pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
            p++;
            goto analyze_url;
        }
        /* scheme://host:port/ */
        p_url->host = copyPath(qq, q - 1 - qq,
            COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
        tmp = Strnew_charp_n(q, p - q);
        p_url->port = atoi(tmp->ptr);
        /* *p is one of ['\0', '/', '?', '#'] */
        break;
    }
    case '@':
        /* scheme://user@...            */
        p_url->user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        p++;
        goto analyze_url;
    case '\0':
        /* scheme://host                */
    case '/':
    case '?':
    case '#':
        p_url->host = copyPath(q, p - q,
            COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
        p_url->port = getSchemeInfo(p_url->scheme).port;
        break;
    }
analyze_file:
#ifndef SUPPORT_NETBIOS_SHARE
    if (p_url->scheme == SCM_LOCAL && p_url->user == NULL && p_url->host != NULL && *p_url->host != '\0' && !is_localhost(p_url->host)) {
        /*
         * In the environments other than CYGWIN, a URL like
         * file://host/file is regarded as ftp://host/file.
         * On the other hand, file://host/file on CYGWIN is
         * regarded as local access to the file //host/file.
         * `host' is a netbios-hostname, drive, or any other
         * name; It is CYGWIN system call who interprets that.
         */

        p_url->scheme = SCM_FTP; /* ftp://host/... */
        if (p_url->port == 0) {
            p_url->port = getSchemeInfo(SCM_FTP).port;
        }
    }
#endif
    if ((*p == '\0' || *p == '#' || *p == '?') && p_url->host == NULL) {
        p_url->file = "";
        goto do_query;
    }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (p_url->scheme == SCM_LOCAL) {
        q = p;
        if (*q == '/')
            q++;
        if (IS_ALPHA(q[0]) && (q[1] == ':' || q[1] == '|')) {
            if (q[1] == '|') {
                p = allocStr(q, -1);
                p[1] = ':';
            } else
                p = q;
        }
    }
#endif

    q = p;
    if (*p == '/')
        p++;
    if (*p == '\0' || *p == '#' || *p == '?') { /* scheme://host[:port]/ */
        p_url->file = DefaultFile(p_url->scheme);
        goto do_query;
    }
    {
        char* cgi = strchr(p, '?');
    again:
        while (*p && *p != '#' && p != cgi)
            p++;
        if (*p == '#' && p_url->scheme == SCM_LOCAL) {
            /*
             * According to RFC2396, # means the beginning of
             * URI-reference, and # should be escaped.  But,
             * if the scheme is SCM_LOCAL, the special
             * treatment will apply to # for convinience.
             */
            if (p > q && *(p - 1) == '/' && (cgi == NULL || p < cgi)) {
                /*
                 * # comes as the first character of the file name
                 * that means, # is not a label but a part of the file
                 * name.
                 */
                p++;
                goto again;
            } else if (*(p + 1) == '\0') {
                /*
                 * # comes as the last character of the file name that
                 * means, # is not a label but a part of the file
                 * name.
                 */
                p++;
            }
        }
        if (p_url->scheme == SCM_LOCAL || p_url->scheme == SCM_MISSING)
            p_url->file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
        else
            p_url->file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }

do_query:
    if (*p == '?') {
        q = ++p;
        while (*p && *p != '#')
            p++;
        p_url->query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    }
do_label:
    if (p_url->scheme == SCM_MISSING) {
        p_url->scheme = SCM_LOCAL;
        p_url->file = allocStr(p, -1);
        p_url->label = NULL;
    } else if (*p == '#')
        p_url->label = allocStr(p + 1, -1);
    else
        p_url->label = NULL;
}

#define ALLOC_STR(s) ((s) == NULL ? NULL : allocStr(s, -1))

void copyParsedURL(ParsedURL* p, const ParsedURL* q)
{
    if (q == NULL) {
        memset(p, 0, sizeof(ParsedURL));
        p->scheme = SCM_UNKNOWN;
        return;
    }
    p->scheme = q->scheme;
    p->port = q->port;
    p->is_nocache = q->is_nocache;
    p->user = ALLOC_STR(q->user);
    p->pass = ALLOC_STR(q->pass);
    p->host = ALLOC_STR(q->host);
    p->file = ALLOC_STR(q->file);
    p->real_file = ALLOC_STR(q->real_file);
    p->label = ALLOC_STR(q->label);
    p->query = ALLOC_STR(q->query);
}

void parseURL2(const char* url, ParsedURL* pu, ParsedURL* current)
{
    int relative_uri = false;

    parseURL(url, pu, current);

    if (pu->scheme == SCM_MAILTO)
        return;

    if (pu->scheme == SCM_DATA)
        return;

    const char* p;
    if (pu->scheme == SCM_NEWS || pu->scheme == SCM_NEWS_GROUP) {
        if (pu->file && !strchr(pu->file, '@') && (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') || *(p + 1) == '\0'))
            pu->scheme = SCM_NEWS_GROUP;
        else
            pu->scheme = SCM_NEWS;
        return;
    }
    if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NNTP_GROUP) {
        if (pu->file && *pu->file == '/')
            pu->file = allocStr(pu->file + 1, -1);
        if (pu->file && !strchr(pu->file, '@') && (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') || *(p + 1) == '\0'))
            pu->scheme = SCM_NNTP_GROUP;
        else
            pu->scheme = SCM_NNTP;
        if (current && (current->scheme == SCM_NNTP || current->scheme == SCM_NNTP_GROUP)) {
            if (pu->host == NULL) {
                pu->host = current->host;
                pu->port = current->port;
            }
        }
        return;
    }
    if (pu->scheme == SCM_LOCAL) {
        const char* q = expandName(file_unquote(pu->file));
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        Str drive;
        if (IS_ALPHA(q[0]) && q[1] == ':') {
            drive = Strnew_charp_n(q, 2);
            Strcat_charp(drive, file_quote(q + 2));
            pu->file = drive->ptr;
        } else
#endif
            pu->file = file_quote(q);
    }

    if (current && (pu->scheme == current->scheme || (pu->scheme == SCM_FTP && current->scheme == SCM_FTPDIR) || (pu->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI))
        && pu->host == NULL) {
        /* Copy omitted element from the current URL */
        pu->user = current->user;
        pu->pass = current->pass;
        pu->host = current->host;
        pu->port = current->port;
        if (pu->file && *pu->file) {
            if (
                pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
                && !(pu->scheme == SCM_LOCAL && IS_ALPHA(pu->file[0])
                    && pu->file[1] == ':')
#endif
            ) {
                /* file is relative [process 1] */
                p = pu->file;
                if (current->file) {
                    Str tmp;
                    tmp = Strnew_charp(current->file);
                    while (tmp->length > 0) {
                        if (Strlastchar(tmp) == '/')
                            break;
                        Strshrink(tmp, 1);
                    }
                    Strcat_charp(tmp, p);
                    pu->file = tmp->ptr;
                    relative_uri = true;
                }
            }
        } else { /* scheme:[?query][#label] */
            pu->file = current->file;
            if (!pu->query)
                pu->query = current->query;
        }
        /* comment: query part need not to be completed
         * from the current URL. */
    }
    if (pu->file) {
        if (pu->scheme == SCM_LOCAL && pu->file[0] != '/' &&
#ifdef SUPPORT_DOS_DRIVE_PREFIX /* for 'drive:' */
            !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':') &&
#endif
            strcmp(pu->file, "-")) {
            /* local file, relative path */
            Str tmp = Strnew_charp(CurrentDir);
            if (Strlastchar(tmp) != '/')
                Strcat_char(tmp, '/');
            Strcat_charp(tmp, file_unquote(pu->file));
            pu->file = file_quote(cleanupName(tmp->ptr));
        } else if (pu->scheme == SCM_HTTP
            || pu->scheme == SCM_HTTPS) {
            if (relative_uri) {
                /* In this case, pu->file is created by [process 1] above.
                 * pu->file may contain relative path (for example,
                 * "/foo/../bar/./baz.html"), cleanupName() must be applied.
                 * When the entire abs_path is given, it still may contain
                 * elements like `//', `..' or `.' in the pu->file. It is
                 * server's responsibility to canonicalize such path.
                 */
                pu->file = cleanupName(pu->file);
            }
        } else if (
            pu->file[0] == '/') {
            /*
             * this happens on the following conditions:
             * (1) ftp scheme (2) local, looks like absolute path.
             * In both case, there must be no side effect with
             * cleanupName(). (I hope so...)
             */
            pu->file = cleanupName(pu->file);
        }
        if (pu->scheme == SCM_LOCAL) {
#ifdef SUPPORT_NETBIOS_SHARE
            if (pu->host && !is_localhost(pu->host)) {
                Str tmp = Strnew_charp("//");
                Strcat_m_charp(tmp, pu->host,
                    cleanupName(file_unquote(pu->file)), NULL);
                pu->real_file = tmp->ptr;
            } else
#endif
                pu->real_file = cleanupName(file_unquote(pu->file));
        }
    }
}

Str _parsedURL2Str(ParsedURL* pu, int pass, int user, int label)
{
    Str tmp;

    if (pu->scheme == SCM_MISSING) {
        return Strnew_charp("???");
    } else if (pu->scheme == SCM_UNKNOWN) {
        return Strnew_charp(pu->file);
    }
    if (pu->host == NULL && pu->file == NULL && label && pu->label != NULL) {
        /* local label */
        return Sprintf("#%s", pu->label);
    }
    if (pu->scheme == SCM_LOCAL && !strcmp(pu->file, "-")) {
        tmp = Strnew_charp("-");
        if (label && pu->label) {
            Strcat_char(tmp, '#');
            Strcat_charp(tmp, pu->label);
        }
        return tmp;
    }
    tmp = Strnew_charp(getSchemeInfo(pu->scheme).name);
    Strcat_char(tmp, ':');
#ifndef USE_W3MMAILER
    if (pu->scheme == SCM_MAILTO) {
        Strcat_charp(tmp, pu->file);
        if (pu->query) {
            Strcat_char(tmp, '?');
            Strcat_charp(tmp, pu->query);
        }
        return tmp;
    }
#endif
    if (pu->scheme == SCM_DATA) {
        Strcat_charp(tmp, pu->file);
        return tmp;
    }
    {
        Strcat_charp(tmp, "//");
    }
    if (user && pu->user) {
        Strcat_charp(tmp, pu->user);
        if (pass && pu->pass) {
            Strcat_char(tmp, ':');
            Strcat_charp(tmp, pu->pass);
        }
        Strcat_char(tmp, '@');
    }
    if (pu->host) {
        Strcat_charp(tmp, pu->host);
        if (pu->port != getSchemeInfo(pu->scheme).port) {
            Strcat_char(tmp, ':');
            Strcat(tmp, Sprintf("%d", pu->port));
        }
    }
    if (
        (pu->file == NULL || (pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
             && !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':' && pu->host == NULL)
#endif
                 )))
        Strcat_char(tmp, '/');
    Strcat_charp(tmp, pu->file);
    if (pu->scheme == SCM_FTPDIR && Strlastchar(tmp) != '/')
        Strcat_char(tmp, '/');
    if (pu->query) {
        Strcat_char(tmp, '?');
        Strcat_charp(tmp, pu->query);
    }
    if (label && pu->label) {
        Strcat_char(tmp, '#');
        Strcat_charp(tmp, pu->label);
    }
    return tmp;
}

Str parsedURL2Str(ParsedURL* pu)
{
    return _parsedURL2Str(pu, false, true, true);
}

Str parsedURL2RefererStr(ParsedURL* pu)
{
    return _parsedURL2Str(pu, false, false, false);
}

void init_stream(struct URLFile* uf, int scheme, InputStream stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->encoding = ENC_7BIT;
    uf->is_cgi = false;
    uf->compression = CMP_NOCOMPRESS;
    uf->content_encoding = CMP_NOCOMPRESS;
    uf->guess_type = NULL;
    uf->ext = NULL;
    uf->modtime = -1;
}

static int
domain_match(char* pat, char* domain)
{
    if (domain == NULL)
        return 0;
    if (*pat == '.')
        pat++;
    for (;;) {
        if (!strcasecmp(pat, domain))
            return 1;
        domain = strchr(domain, '.');
        if (domain == NULL)
            return 0;
        domain++;
    }
}

int check_no_proxy(char* domain)
{
    TextListItem* tl;
    volatile int ret = 0;
    MySignalHandler (*volatile prevtrap)(int _dummy) = NULL;

    if (NO_proxy_domains == NULL || NO_proxy_domains->nitem == 0 || domain == NULL)
        return 0;
    for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
        if (domain_match(tl->ptr, domain))
            return 1;
    }
    if (!NOproxy_netaddr) {
        return 0;
    }
    /*
     * to check noproxy by network addr
     */
    if (sigsetjmp(AbortLoading, 1) != 0) {
        ret = 0;
        goto end;
    }
    TRAP_ON;
    {
        int error;
        struct addrinfo hints;
        struct addrinfo *res, *res0;
        char addr[4 * 16];
        int* af;

        for (af = ai_family_order_table[DNS_order];; af++) {
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = *af;
            error = getaddrinfo(domain, NULL, &hints, &res0);
            if (error) {
                if (*af == PF_UNSPEC) {
                    break;
                }
                /* try next */
                continue;
            }
            for (res = res0; res != NULL; res = res->ai_next) {
                switch (res->ai_family) {
                case AF_INET:
                    inet_ntop(AF_INET,
                        &((struct sockaddr_in*)res->ai_addr)->sin_addr,
                        addr, sizeof(addr));
                    break;
                case AF_INET6:
                    inet_ntop(AF_INET6,
                        &((struct sockaddr_in6*)res->ai_addr)->sin6_addr, addr, sizeof(addr));
                    break;
                default:
                    /* unknown */
                    continue;
                }
                for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
                    if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0) {
                        freeaddrinfo(res0);
                        ret = 1;
                        goto end;
                    }
                }
            }
            freeaddrinfo(res0);
            if (*af == PF_UNSPEC) {
                break;
            }
        }
    }
end:
    TRAP_OFF;
    return ret;
}

const char* filename_extension(const char* path, int is_url)
{
    char *last_dot = "", *p = path;
    int i;

    if (path == NULL)
        return last_dot;
    if (*p == '.')
        p++;
    for (; *p; p++) {
        if (*p == '.') {
            last_dot = p;
        } else if (is_url && *p == '?')
            break;
    }
    if (*last_dot == '.') {
        for (i = 1; i < 8 && last_dot[i]; i++) {
            if (is_url && !IS_ALNUM(last_dot[i]))
                break;
        }
        return allocStr(last_dot, i);
    } else
        return last_dot;
}

ParsedURL*
schemeToProxy(int scheme)
{
    ParsedURL* pu = NULL; /* for gcc */
    switch (scheme) {
    case SCM_HTTP:
        pu = &HTTP_proxy_parsed;
        break;
    case SCM_HTTPS:
        pu = &HTTPS_proxy_parsed;
        break;
#ifdef DEBUG
    default:
        abort();
#endif
    }
    return pu;
}

wc_ces
url_to_charset(const char* url, const ParsedURL* base, wc_ces doc_charset)
{
    const ParsedURL* pu;
    ParsedURL pu_buf;
    if (url && *url && *url != '#') {
        parseURL2((char*)url, &pu_buf, (ParsedURL*)base);
        pu = &pu_buf;
    } else {
        pu = base;
    }
    if (pu && (pu->scheme == SCM_LOCAL || pu->scheme == SCM_LOCAL_CGI))
        return SystemCharset;

    const wc_ces* csptr;
    csptr = query_SCONF_URL_CHARSET(pu);

    return (csptr && *csptr) ? *csptr : doc_charset ? doc_charset
                                                    : DocumentCharset;
}

char* url_encode(const char* url, const ParsedURL* base, wc_ces doc_charset)
{
    return url_quote_conv((char*)url,
        url_to_charset(url, base, doc_charset));
}

char* url_decode2(const char* url, const Buffer* buf)
{
    wc_ces url_charset;

    if (!DecodeURL)
        return (char*)url;
    url_charset = buf ? url_to_charset(url, baseURL((Buffer*)buf), buf->document_charset) : url_to_charset(url, NULL, 0);
    return url_unquote_conv((char*)url, url_charset);
}

int same_url_p(ParsedURL* pu1, ParsedURL* pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port && (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1)
        && (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

char* file_to_url(const char* file)
{
    Str tmp;
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    char* drive = NULL;
#endif
#ifdef SUPPORT_NETBIOS_SHARE
    char* host = NULL;
#endif

    if (!(file = expandPath(file)))
        return NULL;
#ifdef SUPPORT_NETBIOS_SHARE
    if (file[0] == '/' && file[1] == '/') {
        char* p;
        file += 2;
        if (*file) {
            p = strchr(file, '/');
            if (p != NULL && p != file) {
                host = allocStr(file, (p - file));
                file = p;
            }
        }
    }
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (IS_ALPHA(file[0]) && file[1] == ':') {
        drive = allocStr(file, 2);
        file += 2;
    } else
#endif
        if (file[0] != '/') {
        tmp = Strnew_charp(CurrentDir);
        if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, file);
        file = tmp->ptr;
    }
    tmp = Strnew_charp("file://");
#ifdef SUPPORT_NETBIOS_SHARE
    if (host)
        Strcat_charp(tmp, host);
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (drive)
        Strcat_charp(tmp, drive);
#endif
    Strcat_charp(tmp, file_quote(cleanupName(file)));
    return tmp->ptr;
}

char* cleanupName(const char* name)
{
    char *buf, *p, *q;

    buf = allocStr(name, -1);
    p = buf;
    q = name;
    while (*q != '\0') {
        if (strncmp(p, "/../", 4) == 0) { /* foo/bar/../FOO */
            if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
                /* ../../       */
                p += 3;
                q += 3;
            } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
                /* ../../../    */
                p += 3;
                q += 3;
            } else {
                while (p != buf && *--p != '/')
                    ; /* ->foo/FOO */
                *p = '\0';
                q += 3;
                strcat(buf, q);
            }
        } else if (strcmp(p, "/..") == 0) { /* foo/bar/..   */
            if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
                /* ../..        */
            } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
                /* ../../..     */
            } else {
                while (p != buf && *--p != '/')
                    ; /* ->foo/ */
                *++p = '\0';
            }
            break;
        } else if (strncmp(p, "/./", 3) == 0) { /* foo/./bar */
            *p = '\0'; /* -> foo/bar           */
            q += 2;
            strcat(buf, q);
        } else if (strcmp(p, "/.") == 0) { /* foo/. */
            *++p = '\0'; /* -> foo/              */
            break;
        } else if (strncmp(p, "//", 2) == 0) { /* foo//bar */
            /* -> foo/bar           */
            *p = '\0';
            q++;
            strcat(buf, q);
        } else {
            p++;
            q++;
        }
    }
    return buf;
}

int is_localhost(const char* host)
{
    if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") || (HostName && !strcasecmp(host, HostName)) || !strcmp(host, "[::1]"))
        return true;
    return false;
}
