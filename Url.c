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
#include <gcstr.h>
#include "regex.h"

const char* ssl_min_version = (NULL);
int ssl_verify_server = (true);
int ssl_path_modified = (false);
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
            && w3m_config.personal_document_root) {
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
                return name;

            extpath = Strnew_m_charp(passent->pw_dir, "/",
                w3m_config.personal_document_root, NULL);
            if (*w3m_config.personal_document_root == '\0' && *p == '/')
                p++;
        } else
            return name;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath->ptr;
    } else {
        return expandPath(p)->ptr;
    }
}

void parseURL2(char* url, struct Url* pu, struct Url* current)
{
    char* p;
    Str tmp;
    int relative_uri = false;

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
        Str drive;
        if (IS_ALPHA(q[0]) && q[1] == ':') {
            drive = Strnew_charp_n(q, 2);
            Strcat_charp(drive, file_quote(q + 2));
            pu->file = drive->ptr;
        } else
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
                && !(pu->scheme == SCM_LOCAL && IS_ALPHA(pu->file[0])
                    && pu->file[1] == ':')) {
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
                    relative_uri = true;
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
            tmp = Strnew_charp(w3m.CurrentDir);
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
        pu->scheme != SCM_NEWS && pu->scheme != SCM_NEWS_GROUP && (pu->file == NULL || (pu->file[0] != '/' && !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':' && pu->host == NULL))))
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

enum UrlScheme getURLScheme(const char** url)
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
    uf->is_cgi = false;
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
        url_to_charset(url, base, doc_charset))
        ->ptr;
}

char* url_decode2(const char* url, wc_ces url_charset)
{
    if (!w3m_config.DecodeURL)
        return (char*)url;

    return url_unquote_conv((char*)url, url_charset);
}

char* url_unquote_conv(const char* url, wc_ces charset)
{
    wc_uint8 old_auto_detect = WcOption.auto_detect;
    Str tmp;
    tmp = Str_url_unquote(Strnew_charp(url), false, true);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}
