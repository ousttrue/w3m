#include "url.h"
#include "w3m_rc.h"
#include "textlist.h"
#include "alloc.h"
#include "indep.h"
#include "etc.h"
#include "siteconf.h"
#include "buffer.h"
#include "myctype.h"
#include <string.h>

struct Url HTTP_proxy_parsed;
struct Url HTTPS_proxy_parsed;
struct Url FTP_proxy_parsed;

#ifdef __WATT32__
#define write(a, b, c) write_s(a, b, c)
#endif /* __WATT32__ */

#ifdef __MINGW32_VERSION
#define write(a, b, c) send(a, b, c, 0)
#define close(fd) closesocket(fd)
#endif

struct KeyValue {
    const char* item1;
    const char* item2;
};

static struct KeyValue DefaultGuess[] = {
    { "html", "text/html" },
    { "htm", "text/html" },
    { "shtml", "text/html" },
    { "xhtml", "application/xhtml+xml" },
    { "gif", "image/gif" },
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "png", "image/png" },
    { "xbm", "image/xbm" },
    { "au", "audio/basic" },
    { "gz", "application/x-gzip" },
    { "Z", "application/x-compress" },
    { "bz2", "application/x-bzip" },
    { "tar", "application/x-tar" },
    { "zip", "application/x-zip" },
    { "lha", "application/x-lha" },
    { "lzh", "application/x-lha" },
    { "ps", "application/postscript" },
    { "pdf", "application/pdf" },
    { NULL, NULL }
};

/* #define HTTP_DEFAULT_FILE    "/index.html" */

#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

static struct TextList* mimetypes_list;
static struct KeyValue** UserMimeTypes;

static struct KeyValue*
loadMimeTypes(char* filename)
{
    FILE* f;
    char *d, *type;
    int i, n;
    Str tmp;
    struct KeyValue* mtypes;

    f = fopen(expandPath(filename), "r");
    if (f == NULL)
        return NULL;
    n = 0;
    while (tmp = Strfgets(f), tmp->length > 0) {
        d = tmp->ptr;
        if (d[0] != '#') {
            d = strtok(d, " \t\n\r");
            if (d != NULL) {
                d = strtok(NULL, " \t\n\r");
                for (i = 0; d != NULL; i++)
                    d = strtok(NULL, " \t\n\r");
                n += i;
            }
        }
    }
    fseek(f, 0, 0);
    mtypes = New_N(struct KeyValue, n + 1);
    i = 0;
    while (tmp = Strfgets(f), tmp->length > 0) {
        d = tmp->ptr;
        if (d[0] == '#')
            continue;
        type = strtok(d, " \t\n\r");
        if (type == NULL)
            continue;
        while (1) {
            d = strtok(NULL, " \t\n\r");
            if (d == NULL)
                break;
            mtypes[i].item1 = Strnew_charp(d)->ptr;
            mtypes[i].item2 = Strnew_charp(type)->ptr;
            i++;
        }
    }
    mtypes[i].item1 = NULL;
    mtypes[i].item2 = NULL;
    fclose(f);
    return mtypes;
}

void initMimeTypes(void)
{
    if (non_null(getRuntime()->mimetypes_files))
        mimetypes_list = make_domain_list(getRuntime()->mimetypes_files);
    else
        mimetypes_list = NULL;
    if (mimetypes_list == NULL)
        return;
    UserMimeTypes = New_N(struct KeyValue*, mimetypes_list->nitem);
    int i = 0;
    for (TextListItem* tl = mimetypes_list->first; tl; i++, tl = tl->next)
        UserMimeTypes[i] = loadMimeTypes(tl->ptr);
}

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

#if SSLEAY_VERSION_NUMBER >= 0x00905100

#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

#ifdef SSL_CTX_set_min_proto_version

#endif /* SSL_CTX_set_min_proto_version */

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

void parseURL(const char* url, struct Url* p_url, const struct Url* current)
{
    const char* q;
    const char* qq;
    Str tmp;

    url = url_quote(url); /* quote 0x01-0x20, 0x7F-0xFF */

    const char* p = url;
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
            p_url->port = getDefaultPort(p_url->scheme);
        else
            p_url->port = 0;
        goto analyze_file;
    }
    /* after here, p begins with // */
    if (p_url->scheme == SCM_LOCAL) { /* file://foo           */
#ifdef __EMX__
        p += 2;
        goto analyze_file;
#else
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
#endif /* __EMX__ */
    }
    p += 2; /* scheme://foo         */
    /*          ^p is here  */
analyze_url:
    q = p;
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
            p_url->port = getDefaultPort(p_url->scheme);
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
            p_url->port = getDefaultPort(SCM_FTP);
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

void parseURL2(const char* url, struct Url* pu, const struct Url* current)
{
    char* p;
    Str tmp;
    int relative_uri = FALSE;

    parseURL(url, pu, current);

    if (pu->scheme == SCM_LOCAL) {
        char* q = expandName(file_unquote(pu->file));
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
#ifdef USE_EXTERNAL_URI_LOADER
            if (pu->scheme == SCM_UNKNOWN
                && strchr(pu->file, ':') == NULL
                && current && (p = strchr(current->file, ':')) != NULL) {
                pu->file = Sprintf("%s:%s",
                    allocStr(current->file,
                        p - current->file),
                    pu->file)
                               ->ptr;
            } else
#endif
                if (pu->file[0] != '/') {
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
#ifdef __EMX__
        if (pu->scheme == SCM_LOCAL) {
            if (strncmp(pu->file, "/$LIB/", 6)) {
                char abs[_MAX_PATH];

                _abspath(abs, file_unquote(pu->file), _MAX_PATH);
                pu->file = file_quote(cleanupName(abs));
            }
        }
#else
        if (pu->scheme == SCM_LOCAL && pu->file[0] != '/' &&
#ifdef SUPPORT_DOS_DRIVE_PREFIX /* for 'drive:' */
            !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':') &&
#endif
            strcmp(pu->file, "-")) {
            /* local file, relative path */
            tmp = Strnew_charp(getRuntime()->CurrentDir);
            if (Strlastchar(tmp) != '/')
                Strcat_char(tmp, '/');
            Strcat_charp(tmp, file_unquote(pu->file));
            pu->file = file_quote(cleanupName(tmp->ptr));
        }
#endif
        else if (pu->scheme == SCM_HTTP
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

Str _parsedURL2Str(struct Url* pu, bool pass, bool user, bool label)
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
    tmp = Strnew_charp(schemeNumToName(pu->scheme));
    Strcat_char(tmp, ':');
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
        if (pu->port != getDefaultPort(pu->scheme)) {
            Strcat_char(tmp, ':');
            Strcat(tmp, Sprintf("%d", pu->port));
        }
    }
    if (
        (pu->file == NULL || (pu->file[0] != '/')))
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

static const char*
guessContentTypeFromTable(struct KeyValue* table, const char* filename)
{
    if (table == NULL)
        return NULL;

    const char* p = &filename[strlen(filename) - 1];
    while (filename < p && *p != '.')
        p--;
    if (p == filename)
        return NULL;
    p++;

    for (struct KeyValue* t = table; t->item1; t++) {
        if (!strcmp(p, t->item1))
            return t->item2;
    }
    for (struct KeyValue* t = table; t->item1; t++) {
        if (!strcasecmp(p, t->item1))
            return t->item2;
    }
    return NULL;
}

const char* guessContentType(const char* filename)
{
    if (filename == NULL)
        return NULL;
    if (mimetypes_list == NULL)
        goto no_user_mimetypes;

    for (int i = 0; i < mimetypes_list->nitem; i++) {
        const char* ret;
        if ((ret = guessContentTypeFromTable(UserMimeTypes[i], filename)) != NULL)
            return ret;
    }

no_user_mimetypes:
    return guessContentTypeFromTable(DefaultGuess, filename);
}

struct TextList*
make_domain_list(const char* domain_list)
{
    Str tmp;
    struct TextList* domains = NULL;

    const char* p = domain_list;
    tmp = Strnew_size(64);
    while (*p) {
        while (*p && IS_SPACE(*p))
            p++;
        Strclear(tmp);
        while (*p && !IS_SPACE(*p) && *p != ',')
            Strcat_char(tmp, *p++);
        if (tmp->length > 0) {
            if (domains == NULL)
                domains = newTextList();
            pushText(domains, tmp->ptr);
        }
        while (*p && IS_SPACE(*p))
            p++;
        if (*p == ',')
            p++;
    }
    return domains;
}

const char* filename_extension(const char* path, int is_url)
{
    const char* last_dot = "";
    const char* p = path;
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

static struct KeyValue** urimethods;
static struct KeyValue default_urimethods[] = {
    { "mailto", "file:///$LIB/w3mmail.cgi?%s" },
    { NULL, NULL }
};

static struct KeyValue*
loadURIMethods(char* filename)
{
    FILE* f;
    int i, n;
    Str tmp;
    struct KeyValue* um;
    char *up, *p;

    f = fopen(expandPath(filename), "r");
    if (f == NULL)
        return NULL;
    i = 0;
    while (tmp = Strfgets(f), tmp->length > 0) {
        if (tmp->ptr[0] != '#')
            i++;
    }
    fseek(f, 0, 0);
    n = i;
    um = New_N(struct KeyValue, n + 1);
    i = 0;
    while (tmp = Strfgets(f), tmp->length > 0) {
        if (tmp->ptr[0] == '#')
            continue;
        while (IS_SPACE(Strlastchar(tmp)))
            Strshrink(tmp, 1);
        for (up = p = tmp->ptr; *p != '\0'; p++) {
            if (*p == ':') {
                um[i].item1 = Strnew_charp_n(up, p - up)->ptr;
                p++;
                break;
            }
        }
        if (*p == '\0')
            continue;
        while (*p != '\0' && IS_SPACE(*p))
            p++;
        um[i].item2 = Strnew_charp(p)->ptr;
        i++;
    }
    um[i].item1 = NULL;
    um[i].item2 = NULL;
    fclose(f);
    return um;
}

void initURIMethods(void)
{
    struct TextList* methodmap_list = NULL;
    TextListItem* tl;
    int i;

    if (non_null(getRuntime()->urimethodmap_files))
        methodmap_list = make_domain_list(getRuntime()->urimethodmap_files);
    if (methodmap_list == NULL)
        return;
    urimethods = New_N(struct KeyValue*, (methodmap_list->nitem + 1));
    for (i = 0, tl = methodmap_list->first; tl; tl = tl->next) {
        urimethods[i] = loadURIMethods(tl->ptr);
        if (urimethods[i])
            i++;
    }
    urimethods[i] = NULL;
}

Str searchURIMethods(struct Url* pu)
{
    struct KeyValue* ump;
    int i;
    Str scheme = NULL;
    Str url;
    char* p;

    if (pu->scheme != SCM_UNKNOWN)
        return NULL; /* use internal */
    if (urimethods == NULL)
        return NULL;
    url = parsedURL2Str(pu);
    for (p = url->ptr; *p != '\0'; p++) {
        if (*p == ':') {
            scheme = Strnew_charp_n(url->ptr, p - url->ptr);
            break;
        }
    }
    if (scheme == NULL)
        return NULL;

    /*
     * RFC2396 3.1. Scheme Component
     * For resiliency, programs interpreting URI should treat upper case
     * letters as equivalent to lower case in scheme names (e.g., allow
     * "HTTP" as well as "http").
     */
    for (i = 0; (ump = urimethods[i]) != NULL; i++) {
        for (; ump->item1 != NULL; ump++) {
            if (strcasecmp(ump->item1, scheme->ptr) == 0) {
                return Sprintf(ump->item2, url_quote(url->ptr));
            }
        }
    }
    for (ump = default_urimethods; ump->item1 != NULL; ump++) {
        if (strcasecmp(ump->item1, scheme->ptr) == 0) {
            return Sprintf(ump->item2, url_quote(url->ptr));
        }
    }
    return NULL;
}

/*
 * RFC2396: Uniform Resource Identifiers (URI): Generic Syntax
 * Appendix A. Collected BNF for URI
 * uric          = reserved | unreserved | escaped
 * reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |
 *                 "$" | ","
 * unreserved    = alphanum | mark
 * mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" |
 *                  "(" | ")"
 * escaped       = "%" hex hex
 */

#define URI_PATTERN "([-;/?:@&=+$,a-zA-Z0-9_.!~*'()]|%[0-9A-Fa-f][0-9A-Fa-f])*"
void chkExternalURIBuffer(struct Buffer* buf)
{
    struct KeyValue* ump;
    for (int i = 0; (ump = urimethods[i]) != NULL; i++) {
        for (; ump->item1 != NULL; ump++) {
            doc_reAnchor(buf_baseUrl(buf), buf->doc, Sprintf("%s:%s", ump->item1, URI_PATTERN)->ptr);
        }
    }
    for (ump = default_urimethods; ump->item1 != NULL; ump++) {
        doc_reAnchor(buf_baseUrl(buf), buf->doc, Sprintf("%s:%s", ump->item1, URI_PATTERN)->ptr);
    }
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
    }
    return pu;
}

enum wc_ces
url_to_charset(const char* url, const struct Url* base, enum wc_ces doc_charset)
{
    const struct Url* pu;
    struct Url pu_buf;
    const enum wc_ces* csptr;

    if (url && *url && *url != '#') {
        parseURL2((char*)url, &pu_buf, (struct Url*)base);
        pu = &pu_buf;
    } else {
        pu = base;
    }
    if (pu && (pu->scheme == SCM_LOCAL || pu->scheme == SCM_LOCAL_CGI))
        return getRuntime()->SystemCharset;
    csptr = query_SCONF_URL_CHARSET(pu);
    return (csptr && *csptr) ? *csptr : doc_charset ? doc_charset
                                                    : getRuntime()->DocumentCharset;
}

char* url_encode(const char* url, const struct Url* base, enum wc_ces doc_charset)
{
    return url_quote_conv((char*)url,
        url_to_charset(url, base, doc_charset));
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
        tmp = Strnew_charp(getRuntime()->CurrentDir);
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
