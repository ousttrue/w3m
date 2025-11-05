#include "Url.h"
#include "fm.h"
#include "etc.h"
#include "file.h"
#include "local_cgi.h"
#include "w3m_runtime.h"
#include "form.h"
#include "rc.h"
#include "indep.h"
#include "dns_order.h"
#include "cookie.h"
#include "display.h"
#include "istream.h"
#include "terms.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <signal.h>
#include <errno.h>

#include <sys/stat.h>

#include "html.h"
#include <gcstr/gcstr.h>
#include "regex.h"

const char* ssl_min_version = (NULL);
int ssl_verify_server = (TRUE);
int ssl_path_modified = (FALSE);
char* ssl_cert_file = (NULL);

TextList* NO_proxy_domains = 0;

/* XXX: note html.h SCM_ */
int DefaultPort[] = {
    80, /* http */
    70, /* gopher */
    21, /* ftp */
    21, /* ftpdir */
    0, /* local - not defined */
    0, /* local-CGI - not defined? */
    0, /* exec - not defined? */
    119, /* nntp */
    119, /* nntp group */
    119, /* news */
    119, /* news group */
    0, /* data - not defined */
    0, /* mailto - not defined */
    443, /* https */
};

struct cmdtable schemetable[] = {
    { "http", SCM_HTTP },
    { "gopher", SCM_GOPHER },
    { "ftp", SCM_FTP },
    { "local", SCM_LOCAL },
    { "file", SCM_LOCAL },
    /*  {"exec", SCM_EXEC}, */
    { "nntp", SCM_NNTP },
    /*  {"nntp", SCM_NNTP_GROUP}, */
    { "news", SCM_NEWS },
    /*  {"news", SCM_NEWS_GROUP}, */
    { "data", SCM_DATA },
    { "mailto", SCM_MAILTO },
    { "https", SCM_HTTPS },
    { NULL, SCM_UNKNOWN },
};

char* schemeNumToName(int scheme)
{
    int i;

    for (i = 0; schemetable[i].cmdname != NULL; i++) {
        if (schemetable[i].cmd == scheme)
            return schemetable[i].cmdname;
    }
    return NULL;
}
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
    case SCM_GOPHER:
        return allocStr("1", -1);
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
    case SCM_FTP:
    case SCM_FTPDIR:
        return allocStr("/", -1);
    }
    return NULL;
}

#define COPYPATH_SPC_ALLOW 0
#define COPYPATH_SPC_IGNORE 1
#define COPYPATH_SPC_REPLACE 2
#define COPYPATH_SPC_MASK 3
#define COPYPATH_LOWERCASE 4

static char*
copyPath(char* orgpath, int length, int option)
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

void parseURL(char* url, struct Url* p_url, struct Url* current)
{
    char *p, *q, *qq;
    Str tmp;

    url = url_quote(url); /* quote 0x01-0x20, 0x7F-0xFF */

    p = url;
    copyParsedURL(p_url, NULL);
    p_url->scheme = SCM_MISSING;

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    if (*url == '\0' || *url == '#') {
        if (current)
            copyParsedURL(p_url, current);
        goto do_label;
    }
#if defined(__EMX__) || defined(__CYGWIN__)
    if (!strncasecmp(url, "file://localhost/", 17)) {
        p_url->scheme = SCM_LOCAL;
        p += 17 - 1;
        url += 17 - 1;
    }
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (IS_ALPHA(*p) && (p[1] == ':' || p[1] == '|')) {
        p_url->scheme = SCM_LOCAL;
        goto analyze_file;
    }
#endif /* SUPPORT_DOS_DRIVE_PREFIX */
    /* search for scheme */
    p_url->scheme = getURLScheme(&p);
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
            case SCM_NNTP:
            case SCM_NNTP_GROUP:
                p_url->scheme = SCM_NNTP;
                break;
            case SCM_NEWS:
            case SCM_NEWS_GROUP:
                p_url->scheme = SCM_NEWS;
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
        if (p_url->scheme != SCM_UNKNOWN)
            p_url->port = DefaultPort[p_url->scheme];
        else
            p_url->port = 0;
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
    q = p;
    if (*q == '[') { /* rfc2732,rfc2373 compliance */
        p++;
        while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
            p++;
        if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == NULL))
            p = q;
    }
    while (*p && strchr(":/@?#", *p) == NULL)
        p++;
    switch (*p) {
    case ':':
        /* scheme://user:pass@host or
         * scheme://host:port
         */
        qq = q;
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
        if (p_url->scheme != SCM_UNKNOWN)
            p_url->port = DefaultPort[p_url->scheme];
        else
            p_url->port = 0;
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
        if (p_url->port == 0)
            p_url->port = DefaultPort[SCM_FTP];
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
    if (p_url->scheme == SCM_GOPHER) {
        if (*q == '/')
            q++;
        if (*q && q[0] != '/' && q[1] != '/' && q[2] == '/')
            q++;
    }
    if (*p == '/')
        p++;
    if (*p == '\0' || *p == '#' || *p == '?') { /* scheme://host[:port]/ */
        p_url->file = DefaultFile(p_url->scheme);
        goto do_query;
    }
    if (p_url->scheme == SCM_GOPHER && *p == 'R') {
        if (!*++p) {
            p_url->file = "";
            goto do_query;
        }
        tmp = Strnew();
        Strcat_char(tmp, *(p++));
        while (*p && *p != '/')
            p++;
        Strcat_charp(tmp, p);
        while (*p)
            p++;
        p_url->file = copyPath(tmp->ptr, -1, COPYPATH_SPC_IGNORE);
    } else {
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

void copyParsedURL(struct Url* p, const struct Url* q)
{
    if (q == NULL) {
        memset(p, 0, sizeof(struct Url));
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

char* expandName(char* name)
{
    char* p;
    struct passwd *passent, *getpwnam(const char*);
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    p = name;
    if (*p == '/') {
        if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2)))
            && personal_document_root) {
            char* q;
            p += 2;
            q = strchr(p, '/');
            if (q) { /* /~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            } else { /* /~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_m_charp(passent->pw_dir, "/",
                personal_document_root, NULL);
            if (*personal_document_root == '\0' && *p == '/')
                p++;
        } else
            goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath->ptr;
    } else
        return expandPath(p);
rest:
    return name;
}

void parseURL2(char* url, struct Url* pu, struct Url* current)
{
    char* p;
    Str tmp;
    int relative_uri = FALSE;

    parseURL(url, pu, current);
    if (pu->scheme == SCM_MAILTO)
        return;
    if (pu->scheme == SCM_DATA)
        return;
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
        char* q = expandName(file_unquote(pu->file));
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
                pu->scheme != SCM_GOPHER && pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
                && !(pu->scheme == SCM_LOCAL && IS_ALPHA(pu->file[0])
                    && pu->file[1] == ':')
#endif
            ) {
                /* file is relative [process 1] */
                p = pu->file;
                if (current->file) {
                    tmp = Strnew_charp(current->file);
                    while (tmp->length > 0) {
                        if (Strlastchar(tmp) == '/')
                            break;
                        Strshrink(tmp, 1);
                    }
                    Strcat_charp(tmp, p);
                    pu->file = tmp->ptr;
                    relative_uri = TRUE;
                }
            } else if (pu->scheme == SCM_GOPHER && pu->file[0] == '/') {
                p = pu->file;
                pu->file = allocStr(p + 1, -1);
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
            tmp = Strnew_charp(CurrentDir);
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
            pu->scheme != SCM_GOPHER && pu->file[0] == '/') {
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

Str _parsedURL2Str(struct Url* pu, int pass, int user, int label)
{
    Str tmp;
    static char* scheme_str[] = {
        "http",
        "gopher",
        "ftp",
        "ftp",
        "file",
        "file",
        "exec",
        "nntp",
        "nntp",
        "news",
        "news",
        "data",
        "mailto",
        "https",
    };

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
    tmp = Strnew_charp(scheme_str[pu->scheme]);
    Strcat_char(tmp, ':');
    if (pu->scheme == SCM_MAILTO) {
        Strcat_charp(tmp, pu->file);
        if (pu->query) {
            Strcat_char(tmp, '?');
            Strcat_charp(tmp, pu->query);
        }
        return tmp;
    }
    if (pu->scheme == SCM_DATA) {
        Strcat_charp(tmp, pu->file);
        return tmp;
    }
    if (pu->scheme != SCM_NEWS && pu->scheme != SCM_NEWS_GROUP) {
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
        if (pu->port != DefaultPort[pu->scheme]) {
            Strcat_char(tmp, ':');
            Strcat(tmp, Sprintf("%d", pu->port));
        }
    }
    if (
        pu->scheme != SCM_NEWS && pu->scheme != SCM_NEWS_GROUP && (pu->file == NULL || (pu->file[0] != '/'
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

Str parsedURL2Str(struct Url* pu)
{
    return _parsedURL2Str(pu, FALSE, TRUE, TRUE);
}

Str parsedURL2RefererStr(struct Url* pu)
{
    return _parsedURL2Str(pu, FALSE, FALSE, FALSE);
}

int getURLScheme(char** url)
{
    char *p = *url, *q;
    int i;
    int scheme = SCM_MISSING;

    while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
        p++;
    if (*p == ':') { /* scheme found */
        scheme = SCM_UNKNOWN;
        for (i = 0; (q = schemetable[i].cmdname) != NULL; i++) {
            int len = strlen(q);
            if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
                scheme = schemetable[i].cmd;
                *url = p + 1;
                break;
            }
        }
    }
    return scheme;
}

void init_stream(struct URLFile* uf, int scheme, InputStream stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->encoding = ENC_7BIT;
    uf->is_cgi = FALSE;
    uf->compression = CMP_NOCOMPRESS;
    uf->content_encoding = CMP_NOCOMPRESS;
    uf->guess_type = NULL;
    uf->ext = NULL;
    uf->modtime = -1;
}

char* filename_extension(char* path, int is_url)
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

struct Url*
schemeToProxy(int scheme)
{
    struct Url* pu = NULL; /* for gcc */
    switch (scheme) {
    case SCM_HTTP:
        pu = &HTTP_proxy_parsed;
        break;
    case SCM_HTTPS:
        pu = &HTTPS_proxy_parsed;
        break;
    case SCM_FTP:
        pu = &FTP_proxy_parsed;
        break;
    case SCM_GOPHER:
        pu = &GOPHER_proxy_parsed;
        break;
    }
    return pu;
}

wc_ces
url_to_charset(const char* url, const struct Url* base, wc_ces doc_charset)
{
    const struct Url* pu;
    struct Url pu_buf;
    const wc_ces* csptr;

    if (url && *url && *url != '#') {
        parseURL2((char*)url, &pu_buf, (struct Url*)base);
        pu = &pu_buf;
    } else {
        pu = base;
    }
    if (pu && (pu->scheme == SCM_LOCAL || pu->scheme == SCM_LOCAL_CGI))
        return SystemCharset;
    return doc_charset ? doc_charset : DocumentCharset;
}

char* url_encode(const char* url, const struct Url* base, wc_ces doc_charset)
{
    return url_quote_conv((char*)url,
        url_to_charset(url, base, doc_charset));
}

char* url_decode2(const char* url, wc_ces url_charset)
{
    if (!DecodeURL)
        return (char*)url;

    return url_unquote_conv((char*)url, url_charset);
}

char* url_unquote_conv(char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), FALSE, TRUE);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}
