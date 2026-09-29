#include "url.h"
#include "Str.h"
#include "myctype.h"
#include "str_gc.h"
#include <stdlib.h>
#include <string.h>

const char* HostName = NULL;

/* #define HTTP_DEFAULT_FILE    "/index.html" */
#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

struct UrlSchemeInfo {
    const char* name;
    enum UrlScheme scheme;
};

struct UrlSchemeInfo schemetable[] = {
    { "http", SCM_HTTP },
    { "gopher", SCM_GOPHER },
    { "gophers", SCM_GOPHERS },
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
    { 0, SCM_UNKNOWN },
};

/* XXX: note html.h SCM_ */
static int DefaultPort[] = {
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
    70, /* gophers */
    443, /* https */
};

const char* schemeNumToName(enum UrlScheme scheme)
{
    for (int i = 0; schemetable[i].name; i++) {
        if (schemetable[i].scheme == scheme)
            return schemetable[i].name;
    }
    return 0;
}

enum UrlScheme getURLScheme(const char** url)
{
    const char* p = *url;
    enum UrlScheme scheme = SCM_MISSING;

    while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
        p++;
    if (*p == ':') { /* scheme found */
        scheme = SCM_UNKNOWN;
        const char* q;
        for (int i = 0; (q = schemetable[i].name); i++) {
            int len = strlen(q);
            if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
                scheme = schemetable[i].scheme;
                *url = p + 1;
                break;
            }
        }
    }
    return scheme;
}

int getDefaultPort(enum UrlScheme scheme)
{
    return DefaultPort[scheme];
}

bool is_localhost(const char* host)
{
    if (!host
        || !strcasecmp(host, "localhost")
        || !strcmp(host, "127.0.0.1")
        || (HostName && !strcasecmp(host, HostName))
        || !strcmp(host, "[::1]"))
        return true;
    return false;
}
static const char xdigit[0x10] = "0123456789ABCDEF";

struct Url copyParsedURL(const ParsedURL* q)
{
    struct Url url;
    if (q) {
        url = *q;
        url.user = allocStr(q->user, -1);
        url.pass = allocStr(q->pass, -1);
        url.host = allocStr(q->host, -1);
        url.file = allocStr(q->file, -1);
        url.real_file = allocStr(q->real_file, -1);
        url.label = allocStr(q->label, -1);
        url.query = allocStr(q->query, -1);
    } else {
        memset(&url, 0, sizeof(ParsedURL));
        url.scheme = SCM_UNKNOWN;
    }
    return url;
}

Str url_quote(const char* str)
{
    Str tmp = NULL;
    for (const char* p = str; *p; p++) {
        if (is_url_quote(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_char(tmp, '%');
            Strcat_char(tmp, xdigit[((unsigned char)*p >> 4) & 0xF]);
            Strcat_char(tmp, xdigit[(unsigned char)*p & 0xF]);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return Strnew_charp(str);
}

Str url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict(x, InnerCharset, c)->ptr);
}

enum CopyPathOption {
    COPYPATH_SPC_ALLOW,
    COPYPATH_SPC_IGNORE,
    COPYPATH_SPC_REPLACE,
    COPYPATH_SPC_MASK,
    COPYPATH_LOWERCASE,
};

static Str copyPath(const char* orgpath, int length,
    enum CopyPathOption option)
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
        } else {
            Strcat_char(tmp, ch);
        }
        orgpath++;
        length--;
    }
    return tmp;
}

static Str DefaultFile(enum UrlScheme scheme)
{
    switch (scheme) {
    case SCM_HTTP:
    case SCM_HTTPS:
        return Strnew_charp(HTTP_DEFAULT_FILE);
    case SCM_GOPHER:
    case SCM_GOPHERS:
        return Strnew_charp("1");
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
    case SCM_FTP:
    case SCM_FTPDIR:
        return Strnew_charp("/");
    default:
        exit(1);
        return NULL;
    }
}

struct Url parseURL(const char* src, const struct Url* current)
{
    src = url_quote(src)->ptr; /* quote 0x01-0x20, 0x7F-0xFF */
    const char* p = src;

    struct Url url = copyParsedURL(NULL);
    url.scheme = SCM_MISSING;

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    if (*src == '\0' || *src == '#') {
        if (current) {
            url = copyParsedURL(current);
        }
        goto do_label;
    }
    /* search for scheme */
    url.scheme = getURLScheme(&p);
    if (url.scheme == SCM_MISSING) {
        /* scheme part is not found in the url. This means either
         * (a) the url is relative to the current or (b) the url
         * denotes a filename (therefore the scheme is SCM_LOCAL).
         */
        if (current) {
            switch (current->scheme) {
            case SCM_LOCAL:
            case SCM_LOCAL_CGI:
                url.scheme = SCM_LOCAL;
                break;
            case SCM_FTP:
            case SCM_FTPDIR:
                url.scheme = SCM_FTP;
                break;
            case SCM_NNTP:
            case SCM_NNTP_GROUP:
                url.scheme = SCM_NNTP;
                break;
            case SCM_NEWS:
            case SCM_NEWS_GROUP:
                url.scheme = SCM_NEWS;
                break;
            default:
                url.scheme = current->scheme;
                break;
            }
        } else {
            url.scheme = SCM_LOCAL;
        }
        p = src;
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
    if (url.scheme == SCM_UNKNOWN) {
        url.file = allocStr(src, -1);
        return url;
    }
    /* get host and port */
    if (p[0] != '/' || p[1] != '/') { /* scheme:foo or scheme:/foo */
        url.host = NULL;
        if (url.scheme != SCM_UNKNOWN)
            url.port = DefaultPort[url.scheme];
        else
            url.port = 0;
        goto analyze_file;
    }
    /* after here, p begins with // */
    if (url.scheme == SCM_LOCAL) { /* file://foo           */
        if (p[2] == '/' || p[2] == '~'
            /* <A HREF="file:///foo">file:///foo</A>  or <A HREF="file://~user">file://~user</A> */
        ) {
            p += 2;
            goto analyze_file;
        }
    }
    p += 2; /* scheme://foo         */
    /*          ^p is here  */
analyze_url:
    const char* q = p;
#ifdef INET6
    if (*q == '[') { /* rfc2732,rfc2373 compliance */
        p++;
        while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
            p++;
        if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == NULL))
            p = q;
    }
#endif
    while (*p && strchr(":/@?#", *p) == NULL)
        p++;
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
            url.user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE)->ptr;
            url.pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW)->ptr;
            p++;
            goto analyze_url;
        }
        /* scheme://host:port/ */
        url.host = copyPath(qq, q - 1 - qq,
            COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE)
                       ->ptr;
        Str tmp = Strnew_charp_n(q, p - q);
        url.port = atoi(tmp->ptr);
        /* *p is one of ['\0', '/', '?', '#'] */
        break;
    }
    case '@':
        /* scheme://user@...            */
        url.user = copyPath(q, p - q, COPYPATH_SPC_IGNORE)->ptr;
        p++;
        goto analyze_url;
    case '\0':
        /* scheme://host                */
    case '/':
    case '?':
    case '#':
        url.host = copyPath(q, p - q,
            COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE)
                       ->ptr;
        if (url.scheme != SCM_UNKNOWN)
            url.port = DefaultPort[url.scheme];
        else
            url.port = 0;
        break;
    }
analyze_file:
    if (url.scheme == SCM_LOCAL && url.user == NULL && url.host != NULL && *url.host != '\0' && !is_localhost(url.host)) {
        /*
         * In the environments other than CYGWIN, a URL like
         * file://host/file is regarded as ftp://host/file.
         * On the other hand, file://host/file on CYGWIN is
         * regarded as local access to the file //host/file.
         * `host' is a netbios-hostname, drive, or any other
         * name; It is CYGWIN system call who interprets that.
         */
        url.scheme = SCM_FTP; /* ftp://host/... */
        if (url.port == 0)
            url.port = DefaultPort[SCM_FTP];
    }
    if ((*p == '\0' || *p == '#' || *p == '?') && url.host == NULL) {
        url.file = "";
        goto do_query;
    }

    q = p;
    if (url.scheme == SCM_GOPHER
        || url.scheme == SCM_GOPHERS) {
        if (*q == '/')
            q++;
        if (*q && q[0] != '/' && q[1] != '/' && q[2] == '/')
            q++;
    }
    if (*p == '/')
        p++;
    if (*p == '\0' || *p == '#' || *p == '?') { /* scheme://host[:port]/ */
        url.file = DefaultFile(url.scheme)->ptr;
        goto do_query;
    }
    if ((url.scheme == SCM_GOPHER
            || url.scheme == SCM_GOPHERS)
        && *p == 'R') {
        if (!*++p) {
            url.file = "";
            goto do_query;
        }
        Str tmp = Strnew();
        Strcat_char(tmp, *(p++));
        while (*p && *p != '/')
            p++;
        Strcat_charp(tmp, p);
        while (*p)
            p++;
        url.file = copyPath(tmp->ptr, -1, COPYPATH_SPC_IGNORE)->ptr;
    } else {
        const char* cgi = strchr(p, '?');
    again:
        while (*p && *p != '#' && p != cgi)
            p++;
        if (*p == '#' && url.scheme == SCM_LOCAL) {
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
        if (url.scheme == SCM_LOCAL || url.scheme == SCM_MISSING)
            url.file = copyPath(q, p - q, COPYPATH_SPC_ALLOW)->ptr;
        else
            url.file = copyPath(q, p - q, COPYPATH_SPC_IGNORE)->ptr;
    }

do_query:
    if (*p == '?') {
        q = ++p;
        while (*p && *p != '#')
            p++;
        url.query = copyPath(q, p - q, COPYPATH_SPC_ALLOW)->ptr;
    }
do_label:
    if (url.scheme == SCM_MISSING) {
        url.scheme = SCM_LOCAL;
        url.file = allocStr(p, -1);
        url.label = NULL;
    } else if (*p == '#')
        url.label = allocStr(p + 1, -1);
    else
        url.label = NULL;

    return url;
}
