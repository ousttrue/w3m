#include "url.h"
#include "Str.h"
#include "quote.h"
#include "myctype.h"
#include <stdlib.h>
#include <string.h>

#define HTTP_DEFAULT_FILE "/"

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
    return 0;
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

void parseURL(const char* _url, struct Url* p_url, struct Url* current)
{
    const char* q = NULL;

    // quote 0x01-0x20, 0x7F-0xFF
    const char* url = url_quote(_url);
    const char* p = url;

    *p_url = (struct Url) {
        .scheme = SCM_MISSING,
        0,
    };

    // RFC1808: Relative Uniform Resource Locators
    // 4.  Resolving Relative URLs
    if (*url == '\0' || *url == '#') {
        if (current)
            *p_url = copyParsedURL(current);
        goto do_label;
    }
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
        p_url->host = 0;
        p_url->port = getSchemeInfo(p_url->scheme).port;
        goto analyze_file;
    }
    /* after here, p begins with // */
    if (p_url->scheme == SCM_LOCAL) { /* file://foo           */
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
    q = p;

    if (*q == '[') { /* rfc2732,rfc2373 compliance */
        p++;
        while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
            p++;
        if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == 0))
            p = q;
    }

    while (*p && strchr(":/@?#", *p) == 0)
        p++;

    Str tmp;
    switch (*p) {
    case ':': {
        /* scheme://user:pass@host or
         * scheme://host:port
         */
        const char* qq = q;
        q = ++p;
        while (*p && strchr("@/?#", *p) == 0)
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

    if (p_url->scheme == SCM_LOCAL && p_url->user == 0 && p_url->host != 0 && *p_url->host != '\0' && !is_localhost(p_url->host)) {
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

    if ((*p == '\0' || *p == '#' || *p == '?') && p_url->host == 0) {
        p_url->file = "";
        goto do_query;
    }

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
            if (p > q && *(p - 1) == '/' && (cgi == 0 || p < cgi)) {
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
        p_url->label = 0;
    } else if (*p == '#')
        p_url->label = allocStr(p + 1, -1);
    else
        p_url->label = 0;
}

#define ALLOC_STR(s) ((s) == 0 ? 0 : allocStr(s, -1))

struct Url copyParsedURL(const struct Url* q)
{
    if (!q) {
        return (struct Url) {
            .scheme = SCM_UNKNOWN,
            0
        };
    }

    return (struct Url) {
        .scheme = q->scheme,
        .user = ALLOC_STR(q->user),
        .pass = ALLOC_STR(q->pass),
        .host = ALLOC_STR(q->host),
        .port = q->port,
        .file = ALLOC_STR(q->file),
        .label = ALLOC_STR(q->label),
        .query = ALLOC_STR(q->query),
        //
        .real_file = ALLOC_STR(q->real_file),
        .is_nocache = q->is_nocache,
    };
}

void parseURL2(const char* url, struct Url* pu, struct Url* current)
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
            if (pu->host == 0) {
                pu->host = current->host;
                pu->port = current->port;
            }
        }
        return;
    }
    //     if (pu->scheme == SCM_LOCAL) {
    //         const char* q = expandName(file_unquote(pu->file));
    //             pu->file = file_quote(q);
    //     }

    if (current && (pu->scheme == current->scheme || (pu->scheme == SCM_FTP && current->scheme == SCM_FTPDIR) || (pu->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI))
        && pu->host == 0) {
        /* Copy omitted element from the current URL */
        pu->user = current->user;
        pu->pass = current->pass;
        pu->host = current->host;
        pu->port = current->port;
        if (pu->file && *pu->file) {
            if (
                pu->file[0] != '/') {
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
        if (pu->scheme == SCM_LOCAL && pu->file[0] != '/' && strcmp(pu->file, "-")) {
            /* local file, relative path */
            Str tmp = current ? parsedURL2Str(current) : Strnew();
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
            pu->real_file = cleanupName(file_unquote(pu->file));
        }
    }
}

Str _parsedURL2Str(struct Url* pu, int pass, int user, int label)
{
    Str tmp;

    if (pu->scheme == SCM_MISSING) {
        return Strnew_charp("???");
    } else if (pu->scheme == SCM_UNKNOWN) {
        return Strnew_charp(pu->file);
    }
    if (pu->host == 0 && pu->file == 0 && label && pu->label != 0) {
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
        (pu->file == 0 || (pu->file[0] != '/')))
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
    return _parsedURL2Str(pu, false, true, true);
}

Str parsedURL2RefererStr(struct Url* pu)
{
    return _parsedURL2Str(pu, false, false, false);
}

const char* filename_extension(const char* path, int is_url)
{
    const char* last_dot = "";
    if (path == 0)
        return last_dot;
    const char* p = path;
    if (*p == '.')
        p++;
    for (; *p; p++) {
        if (*p == '.') {
            last_dot = p;
        } else if (is_url && *p == '?')
            break;
    }
    if (*last_dot == '.') {
        int i;
        for (i = 1; i < 8 && last_dot[i]; i++) {
            if (is_url && !IS_ALNUM(last_dot[i]))
                break;
        }
        return allocStr(last_dot, i);
    } else
        return last_dot;
}

int same_url_p(struct Url* pu1, struct Url* pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port && (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1)
        && (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
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
    if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1")
        /*|| (HostName && !strcasecmp(host, HostName))*/
        || !strcmp(host, "[::1]"))
        return true;
    return false;
}
