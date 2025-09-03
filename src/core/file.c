#define _GNU_SOURCE
#include "file.h"
#include "buffer_loader.h"
#include "ui.h"
#include "tmpfile.h"
#include "compression.h"
#include "istream.h"
#include "HtmlTagParsed.h"
#include "readbuffer.h"
#include "HtmlTagAttribute.h"
#include "html_title.h"
#include "progress.h"
#include "funcname1.h"
#include "indep.h"
#include "table.h"
#include "http.h"
#include "linein.h"
#include "history.h"
#include "downloadlist.h"
#include "keymap.h"
#include "ui.h"
#include "form.h"
#include "mysignal.h"
#include "map.h"
#include "buffer.h"
#include "rc.h"
#include "mailcap.h"
#include "mimehead.h"
#include "str_util.h"
#include "cookie.h"
#include "display.h"
#include "symbol.h"
#include "ctrlcode.h"
#include "screen_effects.h"
#include "auth.h"
#include "image.h"
#include "etc.h"
#include "fm.h"
#include "TermEntry.h"
#include "graphicchar.h"
#include "w3m.h"
#include "tty.h"
#include "screen.h"
#include "local.h"
#include "regex.h"
#include "myctype.h"
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <wtf.h>
#include <unistd.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

static char* guess_filename(char* file);
static int _MoveFile(char* path1, char* path2);
static FILE* lessopen_stream(char* path);

static sigjmp_buf AbortLoading;

;
/* *INDENT-ON* */

#define SAVE_BUF_SIZE 1536

static MySignalHandler KeyAbort(int _dummy)
{
    siglongjmp(AbortLoading, 1);
}

static Buffer*
loadSomething(struct URLFile* f,
    Buffer* (*loadproc)(struct URLFile*, Buffer*), Buffer* defaultbuf)
{
    Buffer* buf;

    if ((buf = loadproc(f, defaultbuf)) == NULL)
        return NULL;

    if (buf->buffername == NULL || buf->buffername[0] == '\0') {
        buf->buffername = getHttpHeaderValue(buf->document_header, "Subject:");
        if (buf->buffername == NULL && buf->filename != NULL)
            buf->buffername = conv_from_system(lastFileName(buf->filename));
    }
    if (buf->currentURL.scheme == SCM_UNKNOWN)
        buf->currentURL.scheme = f->scheme;
    if (f->scheme == SCM_LOCAL && buf->sourcefile == NULL)
        buf->sourcefile = buf->filename;
    if (loadproc == loadHTMLBuffer
        || loadproc == loadImageBuffer)
        buf->type = "text/html";
    else
        buf->type = "text/plain";
    return buf;
}

int dir_exist(char* path)
{
    struct stat stbuf;

    if (path == NULL || *path == '\0')
        return 0;
    if (stat(path, &stbuf) == -1)
        return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

static int
is_dump_text_type(char* type)
{
    struct mailcap* mcap;
    return (type && (mcap = searchExtViewer(type)) && (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int
is_text_type(char* type)
{
    return (type == NULL || type[0] == '\0' || strncasecmp(type, "text/", 5) == 0 || (strncasecmp(type, "application/", 12) == 0 && strstr(type, "xhtml") != NULL) || strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int
is_plain_text_type(char* type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) || (is_text_type(type) && !is_dump_text_type(type)));
}

int is_html_type(const char* type)
{
    return (type && (strcasecmp(type, "text/html") == 0 || strcasecmp(type, "application/xhtml+xml") == 0));
}

static int
setModtime(char* path, time_t modtime)
{
    struct utimbuf t;
    struct stat st;

    if (stat(path, &st) == 0)
        t.actime = st.st_atime;
    else
        t.actime = time(NULL);
    t.modtime = modtime;
    return utime(path, &t);
}

void examineFile(char* path, struct URLFile* uf)
{
    struct stat stbuf;

    uf->guess_type = NULL;
    if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 || NOT_REGULAR(stbuf.st_mode)) {
        uf->stream = NULL;
        return;
    }
    uf->stream = openIS(path);

    if (use_lessopen && getenv("LESSOPEN") != NULL) {
        FILE* fp;
        uf->guess_type = guessContentType(path);
        if (uf->guess_type == NULL)
            uf->guess_type = "text/plain";
        if (is_html_type(uf->guess_type))
            return;
        if ((fp = lessopen_stream(path))) {
            UFclose(uf);
            uf->stream = newFileStream(fp, (void (*)())pclose);
            uf->guess_type = "text/plain";
            return;
        }
    }
    check_compression(uf, path);
    if (uf->compression != CMP_NOCOMPRESS) {
        char* ext = uf->ext;
        const char* t0 = uncompressed_file_type(path, &ext);
        uf->guess_type = (char*)t0;
        uf->ext = ext;
        uncompress_stream(uf, NULL);
        return;
    }
}

/*
 * convert line
 */
Str convertLine(struct URLFile* uf, Str line, enum ConvertLineMode mode, wc_ces* charset, wc_ces doc_charset)
{
    line = wc_Str_conv_with_detect(line, charset, doc_charset, InnerCharset);
    if (mode != RAW_MODE)
        cleanup_line(line, mode);
    return line;
}

#ifdef USE_XFACE
static char*
xface2xpm(char* xface)
{
    Image image;
    ImageCache* cache;
    FILE* f;
    struct stat st;

    SKIP_BLANKS(xface);
    image.url = xface;
    image.ext = ".xpm";
    image.width = 48;
    image.height = 48;
    image.cache = NULL;
    cache = getImage(&image, NULL, IMG_FLAG_AUTO);
    if (cache->loaded & IMG_FLAG_LOADED && !stat(cache->file, &st))
        return cache->file;
    cache->loaded = IMG_FLAG_ERROR;

    f = popen(Sprintf("%s > %s", shell_quote(auxbinFile(XFACE2XPM)),
                  shell_quote(cache->file))
                  ->ptr,
        "w");
    if (!f)
        return NULL;
    fputs(xface, f);
    pclose(f);
    if (stat(cache->file, &st) || !st.st_size)
        return NULL;
    cache->loaded = IMG_FLAG_LOADED | IMG_FLAG_DONT_REMOVE;
    cache->index = 0;
    return cache->file;
}
#endif

static int
skip_auth_token(char** pp)
{
    char* p;
    int first = AUTHCHR_NUL, typ;

    for (p = *pp;; ++p) {
        switch (*p) {
        case '\0':
            goto endoftoken;
        default:
            if ((unsigned char)*p > 037) {
                typ = AUTHCHR_TOKEN;
                break;
            }
            /* thru */
        case '\177':
        case '[':
        case ']':
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '?':
        case '=':
        case ' ':
        case '\t':
        case ',':
            typ = AUTHCHR_SEP;
            break;
        }

        if (!first)
            first = typ;
        else if (first != typ)
            break;
    }
endoftoken:
    *pp = p;
    return first;
}

static Str
extract_auth_val(char** q)
{
    unsigned char* qq = *(unsigned char**)q;
    int quoted = 0;
    Str val = Strnew();

    SKIP_BLANKS(qq);
    if (*qq == '"') {
        quoted = TRUE;
        Strcat_char(val, *qq++);
    }
    while (*qq != '\0') {
        if (quoted && *qq == '"') {
            Strcat_char(val, *qq++);
            break;
        }
        if (!quoted) {
            switch (*qq) {
            case '[':
            case ']':
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '?':
            case '=':
            case ' ':
            case '\t':
                qq++;
            case ',':
                goto end_token;
            default:
                if (*qq <= 037 || *qq == 0177) {
                    qq++;
                    goto end_token;
                }
            }
        } else if (quoted && *qq == '\\')
            Strcat_char(val, *qq++);
        Strcat_char(val, *qq++);
    }
end_token:
    *q = (char*)qq;
    return val;
}

static char*
extract_auth_param(char* q, struct auth_param* auth)
{
    struct auth_param* ap;
    char* p;

    for (ap = auth; ap->name != NULL; ap++) {
        ap->val = NULL;
    }

    while (*q != '\0') {
        SKIP_BLANKS(q);
        for (ap = auth; ap->name != NULL; ap++) {
            size_t len;

            len = strlen(ap->name);
            if (strncasecmp(q, ap->name, len) == 0 && (IS_SPACE(q[len]) || q[len] == '=')) {
                p = q + len;
                SKIP_BLANKS(p);
                if (*p != '=')
                    return q;
                q = p + 1;
                ap->val = extract_auth_val(&q);
                break;
            }
        }
        if (ap->name == NULL) {
            /* skip unknown param */
            int token_type;
            p = q;
            if ((token_type = skip_auth_token(&q)) == AUTHCHR_TOKEN && (IS_SPACE(*q) || *q == '=')) {
                SKIP_BLANKS(q);
                if (*q != '=')
                    return p;
                q++;
                extract_auth_val(&q);
            } else
                return p;
        }
        if (*q != '\0') {
            SKIP_BLANKS(q);
            if (*q == ',')
                q++;
            else
                break;
        }
    }
    return q;
}

static Str
AuthBasicCred(struct http_auth* ha, Str uname, Str pw, ParsedURL* pu,
    struct HttpRequest* hr, FormList* request)
{
    Str s = Strdup(uname);
    Strcat_char(s, ':');
    Strcat(s, pw);
    return Strnew_m_charp("Basic ", base64_encode(s->ptr, s->length)->ptr, NULL);
}

/* *INDENT-OFF* */
struct auth_param none_auth_param[] = {
    { NULL, NULL }
};

struct auth_param basic_auth_param[] = {
    { "realm", NULL },
    { NULL, NULL }
};

#ifdef USE_DIGEST_AUTH
/* RFC2617: 3.2.1 The WWW-Authenticate Response Header
 * challenge        =  "Digest" digest-challenge
 *
 * digest-challenge  = 1#( realm | [ domain ] | nonce |
 *                       [ opaque ] |[ stale ] | [ algorithm ] |
 *                        [ qop-options ] | [auth-param] )
 *
 * domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
 * URI               = absoluteURI | abs_path
 * nonce             = "nonce" "=" nonce-value
 * nonce-value       = quoted-string
 * opaque            = "opaque" "=" quoted-string
 * stale             = "stale" "=" ( "true" | "false" )
 * algorithm         = "algorithm" "=" ( "MD5" | "MD5-sess" |
 *                        token )
 * qop-options       = "qop" "=" <"> 1#qop-value <">
 * qop-value         = "auth" | "auth-int" | token
 */
struct auth_param digest_auth_param[] = {
    { "realm", NULL },
    { "domain", NULL },
    { "nonce", NULL },
    { "opaque", NULL },
    { "stale", NULL },
    { "algorithm", NULL },
    { "qop", NULL },
    { NULL, NULL }
};
#endif
/* for RFC2617: HTTP Authentication */
struct http_auth www_auth[] = {
    { 1, "Basic ", basic_auth_param, AuthBasicCred },
#ifdef USE_DIGEST_AUTH
    { 10, "Digest ", digest_auth_param, AuthDigestCred },
#endif
    {
        0,
        NULL,
        NULL,
        NULL,
    }
};
/* *INDENT-ON* */

static struct http_auth*
findAuthentication(struct http_auth* hauth, Buffer* buf, char* auth_field)
{
    struct http_auth* ha;
    int len = strlen(auth_field), slen;
    TextListItem* i;
    char *p0, *p;

    memset(hauth, 0, sizeof(struct http_auth));
    for (i = buf->document_header->first; i != NULL; i = i->next) {
        if (strncasecmp(i->ptr, auth_field, len) == 0) {
            for (p = i->ptr + len; p != NULL && *p != '\0';) {
                SKIP_BLANKS(p);
                p0 = p;
                for (ha = &www_auth[0]; ha->scheme != NULL; ha++) {
                    slen = strlen(ha->scheme);
                    if (strncasecmp(p, ha->scheme, slen) == 0) {
                        p += slen;
                        SKIP_BLANKS(p);
                        if (hauth->pri < ha->pri) {
                            *hauth = *ha;
                            p = extract_auth_param(p, hauth->param);
                            break;
                        } else {
                            /* weak auth */
                            p = extract_auth_param(p, none_auth_param);
                        }
                    }
                }
                if (p0 == p) {
                    /* all unknown auth failed */
                    int token_type;
                    if ((token_type = skip_auth_token(&p)) == AUTHCHR_TOKEN && IS_SPACE(*p)) {
                        SKIP_BLANKS(p);
                        p = extract_auth_param(p, none_auth_param);
                    } else
                        break;
                }
            }
        }
    }
    return hauth->scheme ? hauth : NULL;
}

static int
same_url_p(ParsedURL* pu1, ParsedURL* pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port && (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1)
        && (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

static int
checkRedirection(ParsedURL* pu)
{
    static ParsedURL* puv = NULL;
    static int nredir = 0;
    static int nredir_size = 0;
    Str tmp;

    if (pu == NULL) {
        nredir = 0;
        nredir_size = 0;
        puv = NULL;
        return TRUE;
    }
    if (nredir >= FollowRedirection) {
        /* FIXME: gettextize? */
        tmp = Sprintf("Number of redirections exceeded %d at %s",
            FollowRedirection, parsedURL2Str(pu)->ptr);
        message(getUI(), MSG_ERR, tmp->ptr);
        return FALSE;
    } else if (nredir_size > 0 && (same_url_p(pu, &puv[(nredir - 1) % nredir_size]) || (!(nredir % 2) && same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
        /* FIXME: gettextize? */
        tmp = Sprintf("Redirection loop detected (%s)",
            parsedURL2Str(pu)->ptr);
        message(getUI(), MSG_ERR, tmp->ptr);
        return FALSE;
    }
    if (!puv) {
        nredir_size = FollowRedirection / 2 + 1;
        puv = New_N(ParsedURL, nredir_size);
        memset(puv, 0, sizeof(ParsedURL) * nredir_size);
    }
    copyParsedURL(&puv[nredir % nredir_size], pu);
    nredir++;
    return TRUE;
}

/*
 * loadGeneralFile: load file to buffer
 */
Buffer*
loadGeneralFile(char* path, ParsedURL* current, const char* referer,
    int flag, FormList* request, bool do_download)
{
    struct URLFile f, *of = NULL;
    ParsedURL pu;
    Buffer* b = NULL;
    Buffer* (*proc)(struct URLFile*, Buffer*) = loadBuffer;
    const char* tpath;
    const char* t = "text/plain";
    const char* p;
    const char* real_type = NULL;
    Buffer* t_buf = NULL;
    // int  searchHeader = SearchHeader;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    TextList* extra_header = newTextList();
    Str uname = NULL;
    Str pwd = NULL;
    Str realm = NULL;
    int add_auth_cookie_flag;
    unsigned char status = HTST_NORMAL;
    struct URLOption url_option;
    Str tmp;
    Str page = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    struct HttpRequest hr;
    ParsedURL* auth_pu;

    tpath = path;
    prevtrap = NULL;
    add_auth_cookie_flag = 0;

    checkRedirection(NULL);

load_doc: {
    const char* sc_redirect;
    parseURL2(tpath, &pu, current);
    sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
    if (sc_redirect && *sc_redirect && checkRedirection(&pu)) {
        tpath = (char*)sc_redirect;
        request = NULL;
        add_auth_cookie_flag = 0;
        current = New(ParsedURL);
        *current = pu;
        status = HTST_NORMAL;
        goto load_doc;
    }
}
    TRAP_OFF;
    url_option.referer = referer;
    url_option.flag = flag;
    f = openURL(tpath, &pu, current, &url_option, request, extra_header, of,
        &hr, &status);
    of = NULL;
    content_charset = 0;
    if (f.stream == NULL) {
        switch (f.scheme) {
        case SCM_LOCAL: {
            struct stat st;
            if (stat(pu.real_file, &st) < 0)
                return NULL;
            if (S_ISDIR(st.st_mode)) {
                if (UseExternalDirBuffer) {
                    Str cmd = Sprintf("%s?dir=%s#current",
                        DirBufferCommand, pu.file);
                    b = loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0,
                        NULL, do_download);
                    if (b != NULL && b != NO_BUFFER) {
                        copyParsedURL(&b->currentURL, &pu);
                        b->filename = b->currentURL.real_file;
                    }
                    return b;
                } else {
                    page = loadLocalDir(pu.real_file);
                    t = "local:directory";
                    charset = SystemCharset;
                }
            }
        } break;
        case SCM_UNKNOWN:
            /* FIXME: gettextize? */
            message(getUI(), MSG_ERR, Sprintf("Unknown URI: %s", parsedURL2Str(&pu)->ptr)->ptr);
            break;

        default:
            break;
        }
        if (page && page->length > 0)
            goto page_loaded;
        return NULL;
    }

    if (status == HTST_MISSING) {
        TRAP_OFF;
        UFclose(&f);
        return NULL;
    }

    /* openURL() succeeded */
    if (sigsetjmp(AbortLoading, 1) != 0) {
        /* transfer interrupted */
        TRAP_OFF;
        if (b)
            discardBuffer(b);
        UFclose(&f);
        return NULL;
    }

    b = NULL;
    if (f.is_cgi) {
        /* local CGI */
        // searchHeader = TRUE;
    }
    if (header_string)
        header_string = NULL;
    TRAP_ON;
    if (pu.scheme == SCM_HTTP || pu.scheme == SCM_HTTPS) {

        term_cbreak();
        /* FIXME: gettextize? */
        message(getUI(), MSG_INFO, Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr);
        // refresh(ttyWriter());

        if (t_buf == NULL)
            t_buf = newBuffer();

        struct HttpResponse response = readHttpResponse(&f, &pu);
        t_buf->document_header = response.headers;
        if (((response.status_code >= 301 && response.status_code <= 303)
                || response.status_code == 307)
            && (p = (char*)getHttpHeaderValue(t_buf->document_header, "Location:")) != NULL
            && checkRedirection(&pu)) {
            /* document moved */
            /* 301: Moved Permanently */
            /* 302: Found */
            /* 303: See Other */
            /* 307: Temporary Redirect (HTTP/1.1) */
            tpath = url_encode(p, NULL, 0);
            request = NULL;
            UFclose(&f);
            current = New(ParsedURL);
            copyParsedURL(current, &pu);
            t_buf = newBuffer();
            t_buf->bufferprop |= BP_REDIRECTED;
            status = HTST_NORMAL;
            goto load_doc;
        }
        struct ContentTypeCharset cc = getContentType(t_buf->document_header);
        t = cc.content_type;
        if (t == NULL && pu.file != NULL) {
            if (!((response.status_code >= 400 && response.status_code <= 407) || (response.status_code >= 500 && response.status_code <= 505)))
                t = guessContentType(pu.file);
        }
        if (t == NULL)
            t = "text/plain";
        if (add_auth_cookie_flag && realm && uname && pwd) {
            /* If authorization is required and passed */
            add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd,
                0);
            add_auth_cookie_flag = 0;
        }
        if ((p = getHttpHeaderValue(t_buf->document_header, "WWW-Authenticate:")) != NULL && response.status_code == 401) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = &pu;
                getAuthCookie(&hauth, "Authorization:", extra_header,
                    auth_pu, &hr, request, &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    TRAP_OFF;
                    goto page_loaded;
                }
                UFclose(&f);
                add_auth_cookie_flag = 1;
                status = HTST_NORMAL;
                goto load_doc;
            }
        }
        if ((p = getHttpHeaderValue(t_buf->document_header, "Proxy-Authenticate:")) != NULL && response.status_code == 407) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf, "Proxy-Authenticate:")
                    != NULL
                && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                auth_pu = schemeToProxy(pu.scheme);
                getAuthCookie(&hauth, "Proxy-Authorization:",
                    extra_header, auth_pu, &hr, request,
                    &uname, &pwd);
                if (uname == NULL) {
                    /* abort */
                    TRAP_OFF;
                    goto page_loaded;
                }
                UFclose(&f);
                add_auth_cookie_flag = 1;
                status = HTST_NORMAL;
                add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
                goto load_doc;
            }
        }
        /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

        if (status == HTST_CONNECT) {
            of = &f;
            goto load_doc;
        }

        f.modtime = mymktime(getHttpHeaderValue(t_buf->document_header, "Last-Modified:"));
    } else if (pu.scheme == SCM_FTP) {
        check_compression(&f, path);
        if (f.compression != CMP_NOCOMPRESS) {
            char* t1 = (char*)uncompressed_file_type(pu.file, NULL);
            real_type = f.guess_type;
            if (t1)
                t = t1;
            else
                t = real_type;
        } else {
            real_type = guessContentType(pu.file);
            if (real_type == NULL)
                real_type = "text/plain";
            t = real_type;
        }
    } else if (pu.scheme == SCM_DATA) {
        t = f.guess_type;
    }
    //     else if (searchHeader) {
    //         searchHeader = SearchHeader = FALSE;
    //         if (t_buf == NULL)
    //             t_buf = newBuffer();
    //         readHeader(&f, t_buf, searchHeader_through, &pu);
    //         if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL && checkRedirection(&pu)) {
    //             /* document moved */
    //             tpath = url_encode(remove_space(p), NULL, 0);
    //             request = NULL;
    //             UFclose(&f);
    //             add_auth_cookie_flag = 0;
    //             current = New(ParsedURL);
    //             copyParsedURL(current, &pu);
    //             t_buf = newBuffer();
    //             t_buf->bufferprop |= BP_REDIRECTED;
    //             status = HTST_NORMAL;
    //             goto load_doc;
    //         }
    // #ifdef AUTH_DEBUG
    //         if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL) {
    //             /* Authentication needed */
    //             struct http_auth hauth;
    //             if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL
    //                 && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
    //                 auth_pu = &pu;
    //                 getAuthCookie(&hauth, "Authorization:", extra_header,
    //                     auth_pu, &hr, request, &uname, &pwd);
    //                 if (uname == NULL) {
    //                     /* abort */
    //                     TRAP_OFF;
    //                     goto page_loaded;
    //                 }
    //                 UFclose(&f);
    //                 add_auth_cookie_flag = 1;
    //                 status = HTST_NORMAL;
    //                 goto load_doc;
    //             }
    //         }
    // #endif /* defined(AUTH_DEBUG) */
    //         t = checkContentType(t_buf);
    //         if (t == NULL)
    //             t = "text/plain";
    //     }
    else if (DefaultType) {
        t = DefaultType;
        DefaultType = NULL;
    } else {
        t = guessContentType(pu.file);
        if (t == NULL)
            t = "text/plain";
        real_type = t;
        if (f.guess_type)
            t = f.guess_type;
    }

    /* XXX: can we use guess_type to give the type to loadHTMLstream
     *      to support default utf8 encoding for XHTML here? */
    f.guess_type = t;

page_loaded:
    if (page) {
        FILE* src;
        if (image_source)
            return NULL;
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src) {
            Str s;
            s = wc_Str_conv_strict(page, InnerCharset, charset);
            Strfputs(s, src);
            fclose(src);
        }
        if (do_download) {
            char* file;
            if (!src)
                return NULL;
            file = guess_filename(pu.file);
            doFileMove(tmp->ptr, file);
            return NO_BUFFER;
        }
        b = loadHTMLString(page);
        if (b) {
            copyParsedURL(&b->currentURL, &pu);
            b->real_scheme = pu.scheme;
            b->real_type = t;
            if (src)
                b->sourcefile = tmp->ptr;
            b->document_charset = charset;
        }
        return b;
    }

    if (real_type == NULL)
        real_type = t;
    proc = loadBuffer;

    current_content_length = 0;
    if ((p = getHttpHeaderValue(t_buf->document_header, "Content-Length:")) != NULL)
        current_content_length = strtoclen(p);
    if (do_download) {
        /* download only */
        char* file;
        TRAP_OFF;
        if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        if (pu.scheme == SCM_LOCAL) {
            struct stat st;
            if (PreserveTimestamp && !stat(pu.real_file, &st))
                f.modtime = st.st_mtime;
            file = conv_from_system(guess_save_name(NULL, pu.real_file));
        } else
            file = guess_save_name(t_buf, pu.file);
        if (doFileSave(f, file) == 0)
            UFhalfclose(&f);
        else
            UFclose(&f);
        return NO_BUFFER;
    }

    if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        uncompress_stream(&f, &pu.real_file);
    } else if (f.compression != CMP_NOCOMPRESS) {
        if (is_text_type(t) || searchExtViewer(t)) {
            if (t_buf == NULL)
                t_buf = newBuffer();
            uncompress_stream(&f, &t_buf->sourcefile);
            uncompressed_file_type(pu.file, &f.ext);
        } else {
            t = (char*)compress_application_type(f.compression);
            f.compression = CMP_NOCOMPRESS;
        }
    }
    if (image_source) {
        Buffer* b = NULL;
        if (IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        if (save2tmp(f, image_source) == 0) {
            b = newBuffer();
            b->sourcefile = image_source;
            b->real_type = t;
        }
        UFclose(&f);
        TRAP_OFF;
        return b;
    }

    if (is_html_type(t))
        proc = loadHTMLBuffer;
    else if (is_plain_text_type(t))
        proc = loadBuffer;
    else if (activeImage && displayImage && !useExtImageViewer && !strncasecmp(t, "image/", 6))
        proc = loadImageBuffer;

    if (t_buf == NULL)
        t_buf = newBuffer();
    copyParsedURL(&t_buf->currentURL, &pu);
    t_buf->filename = pu.real_file ? pu.real_file : pu.file ? conv_to_system(pu.file)
                                                            : NULL;
    t_buf->ssl_certificate = f.ssl_certificate;

    // if (proc == DO_EXTERNAL) {
    //     b = doExternal(f, t, t_buf);
    // } else
    {
        b = loadSomething(&f, proc, t_buf);
    }

    UFclose(&f);
    if (b && b != NO_BUFFER) {
        b->real_scheme = f.scheme;
        b->real_type = real_type;
        if (pu.label) {
            if (proc == loadHTMLBuffer) {
                Anchor* a;
                a = searchURLLabel(b, pu.label);
                if (a != NULL) {
                    gotoLine(b, a->start.line);
                    if (label_topline)
                        b->topLine = lineSkip(b, b->topLine,
                            b->currentLine->linenumber
                                - b->topLine->linenumber,
                            FALSE);
                    b->pos = a->start.pos;
                    arrangeCursor(b);
                }
            } else { /* plain text */
                int l = atoi(pu.label);
                gotoRealLine(b, l);
                b->pos = 0;
                arrangeCursor(b);
            }
        }
    }
    if (header_string)
        header_string = NULL;
    if (b && b != NO_BUFFER)
        preFormUpdateBuffer(b);
    TRAP_OFF;
    return b;
}

#define TAG_IS(s, tag, len) \
    (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static int
is_period_char(unsigned char* ch)
{
    switch (*ch) {
    case ',':
    case '.':
    case ':':
    case ';':
    case '?':
    case '!':
    case ')':
    case ']':
    case '}':
    case '>':
        return 1;
    default:
        return 0;
    }
}

static int
is_beginning_char(unsigned char* ch)
{
    switch (*ch) {
    case '(':
    case '[':
    case '{':
    case '`':
    case '<':
        return 1;
    default:
        return 0;
    }
}

static int
is_word_char(unsigned char* ch)
{
    Lineprop ctype = get_mctype(ch);

    if (ctype & (PC_CTRL | PC_KANJI | PC_UNKNOWN))
        return 0;
    if (ctype & (PC_WCHAR1 | PC_WCHAR2))
        return 1;

    if (IS_ALNUM(*ch))
        return 1;

    switch (*ch) {
    case ',':
    case '.':
    case ':':
    case '\"': /* " */
    case '\'':
    case '$':
    case '%':
    case '*':
    case '+':
    case '-':
    case '@':
    case '~':
    case '_':
        return 1;
    }
    if (*ch == NBSP_CODE)
        return 1;
    return 0;
}

static int
is_combining_char(unsigned char* ch)
{
    Lineprop ctype = get_mctype(ch);

    if (ctype & PC_WCHAR2)
        return 1;
    return 0;
}

int is_boundary(unsigned char* ch1, unsigned char* ch2)
{
    if (!*ch1 || !*ch2)
        return 1;

    if (*ch1 == ' ' && *ch2 == ' ')
        return 0;

    if (*ch1 != ' ' && is_period_char(ch2))
        return 0;

    if (*ch2 != ' ' && is_beginning_char(ch1))
        return 0;

    if (is_combining_char(ch2))
        return 0;
    if (is_word_char(ch1) && is_word_char(ch2))
        return 0;

    return 1;
}

static InputStream _file_lp2;

static Str
file_feed(void)
{
    Str s;
    s = StrISgets(_file_lp2);
    if (s && s->length == 0) {
        ISclose(_file_lp2);
        return NULL;
    }
    return s;
}

// static void
// HTMLlineproc3(Buffer* buf, InputStream stream)
// {
//     _file_lp2 = stream;
//     HTMLlineproc2body(buf, file_feed, -1);
// }

static void
proc_escape(struct readbuffer* obuf, char** str_return)
{
    char *str = *str_return, *estr;
    int ech = getescapechar(str_return);
    int width, n_add = *str_return - str;
    Lineprop mode = PC_ASCII;

    if (ech < 0) {
        *str_return = str;
        proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
        return;
    }
    mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

    estr = conv_entity(ech);
    check_breakpoint(obuf, obuf->flag & RB_SPECIAL, estr);
    width = get_strwidth(estr);
    if (width == 1 && ech == (unsigned char)*estr && ech != '&' && ech != '<' && ech != '>') {
        if (IS_CNTRL(ech))
            mode = PC_CTRL;
        push_charp(obuf, width, estr, mode);
    } else
        push_nchars(obuf, width, str, n_add, mode);
    set_prevchar(obuf->prevchar, estr, strlen(estr));
    obuf->prev_ctype = mode;
}

static int
need_flushline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    Lineprop mode)
{
    char ch;

    if (obuf->flag & RB_PRE_INT) {
        if (obuf->pos > h_env->limit)
            return 1;
        else
            return 0;
    }

    ch = Strlastchar(obuf->line);
    /* if (ch == ' ' && obuf->tag_sp > 0) */
    if (ch == ' ')
        return 0;

    if (obuf->pos > h_env->limit)
        return 1;

    return 0;
}

/* HTML processing first pass */
void HTMLlineproc0(char* line, struct html_feed_environ* h_env, bool internal)
{
    Lineprop mode;
    int cmd;
    struct readbuffer* obuf = h_env->obuf;
    int indent, delta;
    struct HtmlTagParsed* tag;
    Str tokbuf;
    struct table* tbl = NULL;
    struct table_mode* tbl_mode = NULL;
    int tbl_width = 0;
    int is_hangul, prev_is_hangul = 0;

#ifdef DEBUG
    if (w3m_debug) {
        FILE* f = fopen("zzzproc1", "a");
        fprintf(f, "%c%c%c%c",
            (obuf->flag & RB_PREMODE) ? 'P' : ' ',
            (obuf->table_level >= 0) ? 'T' : ' ',
            (obuf->flag & RB_INTXTA) ? 'X' : ' ',
            (obuf->flag & (RB_SCRIPT | RB_STYLE)) ? 'S' : ' ');
        fprintf(f, "HTMLlineproc1(\"%s\",%d,%lx)\n", line, h_env->limit,
            (unsigned long)h_env);
        fclose(f);
    }
#endif

    tokbuf = Strnew();

table_start:
    if (obuf->table_level >= 0) {
        int level = min(obuf->table_level, MAX_TABLE - 1);
        tbl = tables[level];
        tbl_mode = &table_mode[level];
        tbl_width = table_width(h_env, level);
    }

    while (*line != '\0') {
        char *str, *p;
        int is_tag = FALSE;
        int pre_mode = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->pre_mode : obuf->flag;
        int end_tag = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->end_tag : obuf->end_tag;

        if (*line == '<' || obuf->status != R_ST_NORMAL) {
            /*
             * Tag processing
             */
            if (obuf->status == R_ST_EOL)
                obuf->status = R_ST_NORMAL;
            else {
                read_token(h_env->tagbuf, &line, &obuf->status,
                    pre_mode & RB_PREMODE, obuf->status != R_ST_NORMAL);
                if (obuf->status != R_ST_NORMAL)
                    return;
            }
            if (h_env->tagbuf->length == 0)
                continue;
            str = Strdup(h_env->tagbuf)->ptr;
            if (*str == '<') {
                if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
                    is_tag = TRUE;
                else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE | RB_TITLE))) {
                    line = Strnew_m_charp(str + 1, line, NULL)->ptr;
                    str = "&lt;";
                }
            }
        } else {
            read_token(tokbuf, &line, &obuf->status, pre_mode & RB_PREMODE, 0);
            if (obuf->status != R_ST_NORMAL) /* R_ST_AMP ? */
                obuf->status = R_ST_NORMAL;
            str = tokbuf->ptr;
            if (need_number) {
                str = Strnew_m_charp(getLinkNumberStr(-1)->ptr, str, NULL)->ptr;
                need_number = 0;
            }
        }

        if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE | RB_TITLE)) {
            if (is_tag) {
                p = str;
                if ((tag = parse_tag(&p, internal))) {
                    if (tag->tagid == end_tag || (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM)
                        || (pre_mode & RB_TITLE
                            && (tag->tagid == HTML_N_HEAD
                                || tag->tagid == HTML_BODY)))
                        goto proc_normal;
                }
            }
            /* title */
            if (pre_mode & RB_TITLE) {
                feed_title(str);
                continue;
            }
            /* select */
            if (pre_mode & RB_INSELECT) {
                if (obuf->table_level >= 0)
                    goto proc_normal;
                feed_select(str);
                continue;
            }
            if (is_tag) {
                if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<'))) {
                    str = Strnew_charp_n(str, p - str)->ptr;
                    line = Strnew_m_charp(p, line, NULL)->ptr;
                }
                is_tag = FALSE;
                continue;
            }
            if (obuf->table_level >= 0)
                goto proc_normal;
            /* textarea */
            if (pre_mode & RB_INTXTA) {
                feed_textarea(str);
                continue;
            }
            /* script */
            if (pre_mode & RB_SCRIPT)
                continue;
            /* style */
            if (pre_mode & RB_STYLE)
                continue;
        }

    proc_normal:
        if (obuf->table_level >= 0 && tbl && tbl_mode) {
            /*
             * within table: in <table>..</table>, all input tokens
             * are fed to the table renderer, and then the renderer
             * makes HTML output.
             */
            switch (feed_table(tbl, str, tbl_mode, tbl_width, internal)) {
            case 0:
                /* </table> tag */
                obuf->table_level--;
                if (obuf->table_level >= MAX_TABLE - 1)
                    continue;
                end_table(tbl);
                if (obuf->table_level >= 0) {
                    struct table* tbl0 = tables[obuf->table_level];
                    str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
                    if (tbl0->row < 0)
                        continue;
                    pushTable(tbl0, tbl);
                    tbl = tbl0;
                    tbl_mode = &table_mode[obuf->table_level];
                    tbl_width = table_width(h_env, obuf->table_level);
                    feed_table(tbl, str, tbl_mode, tbl_width, TRUE);
                    continue;
                    /* continue to the next */
                }
                if (obuf->flag & RB_DEL)
                    continue;
                /* all tables have been read */
                if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P)) {
                    int indent = h_env->envs[h_env->envc].indent;
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                }
                save_fonteffect(h_env, obuf);
                initRenderTable();
                renderTable(tbl, tbl_width, h_env);
                restore_fonteffect(h_env, obuf);
                obuf->flag &= ~RB_IGNORE_P;
                if (tbl->vspace > 0) {
                    int indent = h_env->envs[h_env->envc].indent;
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag |= RB_IGNORE_P;
                }
                set_space_to_prevchar(obuf->prevchar);
                continue;
            case 1:
                /* <table> tag */
                break;
            default:
                continue;
            }
        }

        if (is_tag) {
            /*** Beginning of a new tag ***/
            if ((tag = parse_tag(&str, internal)))
                cmd = tag->tagid;
            else
                continue;
            /* process tags */
            if (HTMLtagproc1(tag, h_env) == 0) {
                /* preserve the tag for second-stage processing */
                if (tag->need_reconstruct)
                    h_env->tagbuf = parsedtag2str(tag);
                push_tag(obuf, h_env->tagbuf->ptr, cmd);
            } else {
                process_idattr(obuf, cmd, tag);
            }
            obuf->bp.init_flag = 1;
            clear_ignore_p_flag(obuf, cmd);
            if (cmd == HTML_TABLE)
                goto table_start;
            else {
                if (displayLinkNumber && cmd == HTML_A && !internal)
                    if (h_env->obuf->anchor.url)
                        need_number = 1;
                continue;
            }
        }

        if (obuf->flag & (RB_DEL | RB_S))
            continue;
        while (*str) {
            mode = get_mctype(str);
            delta = get_mcwidth(str);
            if (obuf->flag & (RB_SPECIAL & ~RB_NOBR)) {
                char ch = *str;
                if (!(obuf->flag & RB_PLAIN) && (*str == '&')) {
                    char* p = str;
                    int ech = getescapechar(&p);
                    if (ech == '\n' || ech == '\r') {
                        ch = '\n';
                        str = p - 1;
                    } else if (ech == '\t') {
                        ch = '\t';
                        str = p - 1;
                    }
                }
                if (ch != '\n')
                    obuf->flag &= ~RB_IGNORE_P;
                if (ch == '\n') {
                    str++;
                    if (obuf->flag & RB_IGNORE_P) {
                        obuf->flag &= ~RB_IGNORE_P;
                        continue;
                    }
                    if (obuf->flag & RB_PRE_INT)
                        PUSH(obuf, ' ');
                    else
                        flushline(h_env, obuf, h_env->envs[h_env->envc].indent,
                            1, h_env->limit);
                } else if (ch == '\t') {
                    do {
                        PUSH(obuf, ' ');
                    } while ((h_env->envs[h_env->envc].indent + obuf->pos)
                            % Tabstop
                        != 0);
                    str++;
                } else if (obuf->flag & RB_PLAIN) {
                    char* p = html_quote_char(*str);
                    if (p) {
                        push_charp(obuf, 1, p, PC_ASCII);
                        str++;
                    } else {
                        proc_mchar(obuf, 1, delta, &str, mode);
                    }
                } else {
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, 1, delta, &str, mode);
                }
                if (obuf->flag & (RB_SPECIAL & ~RB_PRE_INT))
                    continue;
            } else {
                if (!IS_SPACE(*str))
                    obuf->flag &= ~RB_IGNORE_P;
                if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str)) {
                    if (*obuf->prevchar->ptr != ' ') {
                        PUSH(obuf, ' ');
                    }
                    str++;
                } else {
                    if (mode == PC_KANJI1)
                        is_hangul = wtf_is_hangul((wc_uchar*)str);
                    else
                        is_hangul = 0;
                    if (!SimplePreserveSpace && mode == PC_KANJI1 && !is_hangul && !prev_is_hangul && obuf->pos > h_env->envs[h_env->envc].indent && Strlastchar(obuf->line) == ' ') {
                        while (obuf->line->length >= 2 && !strncmp(obuf->line->ptr + obuf->line->length - 2, "  ", 2)
                            && obuf->pos >= h_env->envs[h_env->envc].indent) {
                            Strshrink(obuf->line, 1);
                            obuf->pos--;
                        }
                        if (obuf->line->length >= 3 && obuf->prev_ctype == PC_KANJI1 && Strlastchar(obuf->line) == ' ' && obuf->pos >= h_env->envs[h_env->envc].indent) {
                            Strshrink(obuf->line, 1);
                            obuf->pos--;
                        }
                    }
                    prev_is_hangul = is_hangul;
                    if (*str == '&')
                        proc_escape(obuf, &str);
                    else
                        proc_mchar(obuf, obuf->flag & RB_SPECIAL, delta, &str,
                            mode);
                }
            }
            if (need_flushline(h_env, obuf, mode)) {
                char* bp = obuf->line->ptr + obuf->bp.len;
                char* tp = bp - obuf->bp.tlen;
                int i = 0;

                if (tp > obuf->line->ptr && tp[-1] == ' ')
                    i = 1;

                indent = h_env->envs[h_env->envc].indent;
                if (obuf->bp.pos - i > indent) {
                    Str line;
                    append_tags(obuf); /* may reallocate the buffer */
                    bp = obuf->line->ptr + obuf->bp.len;
                    line = Strnew_charp(bp);
                    Strshrink(obuf->line, obuf->line->length - obuf->bp.len);
                    if (obuf->pos - i > h_env->limit)
                        obuf->flag |= RB_FILL;
                    back_to_breakpoint(obuf);
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag &= ~RB_FILL;
                    HTMLlineproc0(line->ptr, h_env, true);
                }
            }
        }
    }
    if (!(obuf->flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
        char* tp;
        int i = 0;

        if (obuf->bp.pos == obuf->pos) {
            tp = &obuf->line->ptr[obuf->bp.len - obuf->bp.tlen];
        } else {
            tp = &obuf->line->ptr[obuf->line->length];
        }

        if (tp > obuf->line->ptr && tp[-1] == ' ')
            i = 1;
        indent = h_env->envs[h_env->envc].indent;
        if (obuf->pos - i > h_env->limit) {
            obuf->flag |= RB_FILL;
            flushline(h_env, obuf, indent, 0, h_env->limit);
            obuf->flag &= ~RB_FILL;
        }
    }
}

/*
 * loadHTMLBuffer: read file and make new buffer
 */
Buffer*
loadHTMLBuffer(struct URLFile* f, Buffer* newBuf)
{
    FILE* src = NULL;
    Str tmp;

    if (newBuf == NULL)
        newBuf = newBuffer();
    if (newBuf->sourcefile == NULL && (f->scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, FALSE);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

void init_henv(struct html_feed_environ* h_env, struct readbuffer* obuf,
    struct environment* envs, int nenv, TextLineList* buf,
    int limit, int indent)
{
    envs[0].indent = indent;

    obuf->line = Strnew();
    obuf->cprop = 0;
    obuf->pos = 0;
    obuf->prevchar = Strnew_size(8);
    set_space_to_prevchar(obuf->prevchar);
    obuf->flag = RB_IGNORE_P;
    obuf->flag_sp = 0;
    obuf->status = R_ST_NORMAL;
    obuf->table_level = -1;
    obuf->nobr_level = 0;
    obuf->q_level = 0;
    memset((void*)&obuf->anchor, 0, sizeof(obuf->anchor));
    obuf->img_alt = 0;
    obuf->input_alt.hseq = 0;
    obuf->input_alt.fid = -1;
    obuf->input_alt.in = 0;
    obuf->input_alt.type = NULL;
    obuf->input_alt.name = NULL;
    obuf->input_alt.value = NULL;
    obuf->in_bold = 0;
    obuf->in_italic = 0;
    obuf->in_under = 0;
    obuf->in_strike = 0;
    obuf->in_ins = 0;
    obuf->prev_ctype = PC_ASCII;
    obuf->tag_sp = 0;
    obuf->fontstat_sp = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    obuf->bp.init_flag = 1;
    set_breakpoint(obuf, 0);

    h_env->buf = buf;
    h_env->f = NULL;
    h_env->obuf = obuf;
    h_env->tagbuf = Strnew();
    h_env->limit = limit;
    h_env->maxlimit = 0;
    h_env->envs = envs;
    h_env->nenv = nenv;
    h_env->envc = 0;
    h_env->envc_real = 0;
    h_env->title = NULL;
    h_env->blank_lines = 0;
}

static void
print_internal_information(struct html_feed_environ* henv)
{
    int i;
    Str s;
    TextLineList* tl = newTextLineList();

    s = Strnew_charp("<internal>");
    pushTextLine(tl, newTextLine(s, 0));
    if (henv->title) {
        s = Strnew_m_charp("<title_alt title=\"",
            html_quote(henv->title), "\">", NULL);
        pushTextLine(tl, newTextLine(s, 0));
    }
    if (n_select > 0) {
        FormSelectOptionItem* ip;
        for (i = 0; i < n_select; i++) {
            s = Sprintf("<select_int selectnumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            for (ip = select_option[i].first; ip; ip = ip->next) {
                s = Sprintf("<option_int value=\"%s\" label=\"%s\"%s>",
                    html_quote(ip->value ? ip->value->ptr : ip->label->ptr),
                    html_quote(ip->label->ptr),
                    ip->checked ? " selected" : "");
                pushTextLine(tl, newTextLine(s, 0));
            }
            s = Strnew_charp("</select_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
    if (n_textarea > 0) {
        for (i = 0; i < n_textarea; i++) {
            s = Sprintf("<textarea_int textareanumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            s = Strnew_charp(html_quote(textarea_str[i]->ptr));
            Strcat_charp(s, "</textarea_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
    s = Strnew_charp("</internal>");
    pushTextLine(tl, newTextLine(s, 0));

    if (henv->buf)
        appendTextLineList(henv->buf, tl);
    else if (henv->f) {
        TextLineListItem* p;
        for (p = tl->first; p; p = p->next)
            fprintf(henv->f, "%s\n", Str_conv_to_halfdump(p->ptr->line)->ptr);
    }
}

void loadHTMLstream(struct URLFile* f, Buffer* newBuf, FILE* src, int internal)
{
    Str html = Strnew();
    Str lineBuf2;

    // wc_ces doc_charset = DocumentCharset;
    // wc_ces charset = WC_CES_US_ASCII;
    // if (newBuf != NULL) {
    //     if (newBuf->document_charset)
    //         charset = /*doc_charset =*/newBuf->document_charset;
    // }
    // if (content_charset && UseContentCharset)
    //     doc_charset = content_charset;
    // else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
    //     doc_charset = WC_CES_UTF_8;
    // meta_charset = 0;
    // cur_document_charset = charset;

    while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
        Strcat(html, lineBuf2);
    }

    struct UI ui = getUI();
    loadHTML(html, WC_CES_SHIFT_JIS /*WC_CES_US_ASCII*/, ui.vt->COLS, ui.use_graphic, internal, newBuf);
    //     struct TermEntry* t = getTermEntry();
    //     struct environment envs[MAX_ENV_LEVEL];
    //     long long linelen = 0;
    //     long long trbyte = 0;
    //     Str lineBuf2 = Strnew();
    //     wc_ces charset = WC_CES_US_ASCII;
    //     wc_ces  doc_charset = DocumentCharset;
    //     struct html_feed_environ htmlenv1;
    //     struct readbuffer obuf;
    //     int  image_flag;
    //     MySignalHandler (* prevtrap)(int _dummy) = NULL;
    //
    //     if (graph_ok(t)) {
    //         symbol_width = symbol_width0 = 1;
    //     } else {
    //         symbol_width0 = 0;
    //         get_symbol(DisplayCharset, &symbol_width0);
    //         symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    //     }
    //
    //     init_title();
    //     init2();
    //     if (newBuf->image_flag)
    //         image_flag = newBuf->image_flag;
    //     else if (activeImage && displayImage && autoImage)
    //         image_flag = IMG_FLAG_AUTO;
    //     else
    //         image_flag = IMG_FLAG_SKIP;
    //
    //     init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);
    //
    //     htmlenv1.buf = newTextLineList();
    // #if defined(USE_M17N) || defined(USE_IMAGE)
    //     cur_baseURL = baseURL(newBuf);
    // #endif
    //
    //     if (sigsetjmp(AbortLoading, 1) != 0) {
    //         HTMLlineproc0("<br>Transfer Interrupted!<br>", &htmlenv1, true);
    //         goto phase2;
    //     }
    //     TRAP_ON;
    //
    //     if (newBuf != NULL) {
    //         if (newBuf->document_charset)
    //             charset = doc_charset = newBuf->document_charset;
    //     }
    //     if (content_charset && UseContentCharset)
    //         doc_charset = content_charset;
    //     else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
    //         doc_charset = WC_CES_UTF_8;
    //     meta_charset = 0;
    //     if (IStype(f->stream) != IST_ENCODED)
    //         f->stream = newEncodedStream(f->stream, f->encoding);
    //     while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
    //         if (src)
    //             Strfputs(lineBuf2, src);
    //         linelen += lineBuf2->length;
    //         showProgress(current_content_length, &linelen, &trbyte);
    //         if (meta_charset) { /* <META> */
    //             if (content_charset == 0 && UseContentCharset) {
    //                 doc_charset = meta_charset;
    //                 charset = WC_CES_US_ASCII;
    //             }
    //             meta_charset = 0;
    //         }
    //         lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);
    //         cur_document_charset = charset;
    //         HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
    //     }
    //     if (obuf.status != R_ST_NORMAL) {
    //         HTMLlineproc0("\n", &htmlenv1, internal);
    //     }
    //     obuf.status = R_ST_NORMAL;
    //     completeHTMLstream(&htmlenv1, &obuf);
    //     flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
    // #if defined(USE_M17N) || defined(USE_IMAGE)
    //     cur_baseURL = NULL;
    // #endif
    //     cur_document_charset = 0;
    //     if (htmlenv1.title)
    //         newBuf->buffername = htmlenv1.title;
    // phase2:
    //     newBuf->trbyte = trbyte + linelen;
    //     TRAP_OFF;
    //     newBuf->document_charset = charset;
    //     newBuf->image_flag = image_flag;
    //     HTMLlineproc2(newBuf, htmlenv1.buf);
}

/*
 * loadHTMLString: read string and make new buffer
 */
Buffer*
loadHTMLString(Str page)
{
    struct URLFile f;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    Buffer* newBuf;

    init_stream(&f, SCM_LOCAL, newStrStream(page));

    newBuf = newBuffer();
    if (sigsetjmp(AbortLoading, 1) != 0) {
        TRAP_OFF;
        discardBuffer(newBuf);
        UFclose(&f);
        return NULL;
    }
    TRAP_ON;

    newBuf->document_charset = InnerCharset;
    loadHTMLstream(&f, newBuf, NULL, TRUE);
    newBuf->document_charset = WC_CES_US_ASCII;

    TRAP_OFF;
    UFclose(&f);
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->type = "text/html";
    newBuf->real_type = newBuf->type;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}

/*
 * loadBuffer: read file and make new buffer
 */
Buffer*
loadBuffer(struct URLFile* uf, Buffer* newBuf)
{
    FILE* src = NULL;
    wc_ces charset = WC_CES_US_ASCII;
    wc_ces doc_charset = DocumentCharset;
    Str lineBuf2;
    char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    long long linelen = 0, trbyte = 0;
    Lineprop* propBuffer = NULL;
    Linecolor* colorBuffer = NULL;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;

    if (newBuf == NULL)
        newBuf = newBuffer();

    if (sigsetjmp(AbortLoading, 1) != 0) {
        goto _end;
    }
    TRAP_ON;

    if (newBuf->sourcefile == NULL && (uf->scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmpf = tmpfname(TMPF_SRC, NULL);
        src = fopen(tmpf->ptr, "w");
        if (src)
            newBuf->sourcefile = tmpf->ptr;
    }
    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && UseContentCharset)
        doc_charset = content_charset;

    nlines = 0;
    if (IStype(uf->stream) != IST_ENCODED)
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
    while ((lineBuf2 = StrmyISgets(uf->stream)) && lineBuf2->length) {
        if (src)
            Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        showProgress(current_content_length, &linelen, &trbyte);
        lineBuf2 = convertLine(uf, lineBuf2, HEADER_MODE, &charset, doc_charset);
        if (squeezeBlankLine) {
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
                ++nlines;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        Strchop(lineBuf2);
        lineBuf2 = checkType(lineBuf2, &propBuffer, NULL);
        addnewline(newBuf, lineBuf2->ptr, propBuffer, colorBuffer,
            lineBuf2->length, -1, nlines);
    }
_end:
    TRAP_OFF;
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->trbyte = trbyte + linelen;
    newBuf->document_charset = charset;
    if (src)
        fclose(src);

    return newBuf;
}

Buffer*
loadImageBuffer(struct URLFile* uf, Buffer* newBuf)
{
    Image image;
    ImageCache* cache;
    Str tmp, tmpf;
    FILE* src = NULL;
    struct URLFile f;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    struct stat st;
    const ParsedURL* pu = newBuf ? &newBuf->currentURL : NULL;

    loadImage(newBuf, IMG_FLAG_STOP, false);
    image.url = uf->url;
    image.ext = uf->ext;
    image.width = -1;
    image.height = -1;
    image.cache = NULL;
    cache = getImage(&image, (ParsedURL*)pu, IMG_FLAG_AUTO);
    if (!(pu && pu->is_nocache) && cache->loaded & IMG_FLAG_LOADED && !stat(cache->file, &st))
        goto image_buffer;

    if (IStype(uf->stream) != IST_ENCODED)
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
    TRAP_ON;
    if (save2tmp(*uf, cache->file) < 0) {
        TRAP_OFF;
        return NULL;
    }
    TRAP_OFF;

    cache->loaded = IMG_FLAG_LOADED;
    cache->index = 0;

image_buffer:
    if (newBuf == NULL)
        newBuf = newBuffer();
    cache->loaded |= IMG_FLAG_DONT_REMOVE;
    if (newBuf->sourcefile == NULL && uf->scheme != SCM_LOCAL)
        newBuf->sourcefile = cache->file;

    tmp = Sprintf("<img src=\"%s\"><br><br>", html_quote(image.url));
    tmpf = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmpf->ptr, "w");
    if (src == NULL)
        return NULL;
    newBuf->mailcap_source = tmpf->ptr;

    init_stream(&f, SCM_LOCAL, newStrStream(tmp));
    loadHTMLstream(&f, newBuf, src, TRUE);
    UFclose(&f);
    if (src)
        fclose(src);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->image_flag = IMG_FLAG_AUTO;
    return newBuf;
}

static Str
conv_symbol(Line* l)
{
    Str tmp = NULL;
    char *p = l->lineBuf, *ep = p + l->len;
    Lineprop* pr = l->propBuf;
    int w;
    char** symbol = NULL;

    for (; p < ep; p++, pr++) {
        if (*pr & PC_SYMBOL) {
            char c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
            int len = get_mclen(p);
            if (tmp == NULL) {
                tmp = Strnew_size(l->len);
                Strcopy_charp_n(tmp, l->lineBuf, p - l->lineBuf);
                w = (*pr & PC_KANJI) ? 2 : 1;
                symbol = get_symbol(DisplayCharset, &w);
            }
            Strcat_charp(tmp, symbol[(unsigned char)c % N_SYMBOL]);
            p += len - 1;
            pr += len - 1;
        } else if (tmp != NULL)
            Strcat_char(tmp, *p);
    }
    if (tmp)
        return tmp;
    else
        return Strnew_charp_n(l->lineBuf, l->len);
}

/*
 * saveBuffer: write buffer to file
 */
static void
_saveBuffer(Buffer* buf, Line* l, FILE* f, int cont)
{
    Str tmp;
    int is_html = FALSE;
    int set_charset = !DisplayCharset;
    wc_ces charset = DisplayCharset ? DisplayCharset : WC_CES_US_ASCII;

    is_html = is_html_type(buf->type);
}

void saveBuffer(Buffer* buf, FILE* f, int cont)
{
    _saveBuffer(buf, buf->firstLine, f, cont);
}

void saveBufferBody(Buffer* buf, FILE* f, int cont)
{
    Line* l = buf->firstLine;

    while (l != NULL && l->real_linenumber == 0)
        l = l->next;
    _saveBuffer(buf, l, f, cont);
}

int save2tmp(struct URLFile uf, char* tmpf)
{
    FILE* ff;
    long long linelen = 0, trbyte = 0;
    MySignalHandler (*prevtrap)(int _dummy) = NULL;
    static sigjmp_buf env_bak;
    int retval = 0;
    char* buf = NULL;

    ff = fopen(tmpf, "wb");
    if (ff == NULL) {
        /* fclose(f); */
        return -1;
    }
    memcpy(env_bak, AbortLoading, sizeof(sigjmp_buf));
    if (sigsetjmp(AbortLoading, 1) != 0) {
        goto _end;
    }
    TRAP_ON;
    {
        int count;

        buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
        while ((count = ISread_n(uf.stream, buf, SAVE_BUF_SIZE)) > 0) {
            if (fwrite(buf, 1, count, ff) != count) {
                retval = -2;
                goto _end;
            }
            linelen += count;
            showProgress(current_content_length, &linelen, &trbyte);
        }
    }
_end:
    memcpy(AbortLoading, env_bak, sizeof(sigjmp_buf));
    TRAP_OFF;
    xfree(buf);
    fclose(ff);
    current_content_length = 0;
    return retval;
}

static int
_MoveFile(char* path1, char* path2)
{
    InputStream f1;
    FILE* f2;
    int is_pipe;
    long long linelen = 0, trbyte = 0;
    char* buf = NULL;
    int count;

    f1 = openIS(path1);
    if (f1 == NULL)
        return -1;
    if (*path2 == '|' && PermitSaveToPipe) {
        is_pipe = TRUE;
        f2 = popen(path2 + 1, "w");
    } else {
        is_pipe = FALSE;
        f2 = fopen(path2, "wb");
    }
    if (f2 == NULL) {
        ISclose(f1);
        return -1;
    }
    current_content_length = 0;
    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(f1, buf, SAVE_BUF_SIZE)) > 0) {
        fwrite(buf, 1, count, f2);
        linelen += count;
        showProgress(current_content_length, &linelen, &trbyte);
    }
    xfree(buf);
    ISclose(f1);
    if (is_pipe)
        pclose(f2);
    else
        fclose(f2);
    return 0;
}

int _doFileCopy(char* tmpf, char* defstr, int download)
{
    Str msg;
    Str filen;
    char *p, *q = NULL;
    pid_t pid;
    char* lock;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
    FILE* f;
#endif
    struct stat st;
    long long size = 0;
    int is_pipe = FALSE;

    // if (fmInitialized)
    {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            q = inputLineHist(getUI(), "(Download)Save file to: ",
                defstr, IN_COMMAND, SaveHist);
            if (q == NULL || *q == '\0')
                return FALSE;
            p = conv_to_system(q);
        }
        if (*p == '|' && PermitSaveToPipe)
            is_pipe = TRUE;
        else {
            if (q) {
                p = unescape_spaces(Strnew_charp(q))->ptr;
                p = conv_to_system(p);
            }
            p = expandPath(p);
            if (checkOverWrite(p) < 0)
                return -1;
        }
        if (checkCopyFile(tmpf, p) < 0) {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't copy. %s and %s are identical.",
                conv_from_system(tmpf), conv_from_system(p));
            message(getUI(), MSG_ERR, msg->ptr);
            return -1;
        }
        if (!download) {
            if (_MoveFile(tmpf, p) < 0) {
                /* FIXME: gettextize? */
                msg = Sprintf("Can't save to %s", conv_from_system(p));
                message(getUI(), MSG_ERR, msg->ptr);
            }
            return -1;
        }
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
        symlink(p, lock);
#else
        f = fopen(lock, "w");
        if (f)
            fclose(f);
#endif
        flush_tty();
        pid = fork();
        if (!pid) {
            setup_child(FALSE, 0, -1);
            if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
                setModtime(p, st.st_mtime);
            unlink(lock);
            exit(0);
        }
        if (!stat(tmpf, &st))
            size = st.st_size;
        addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
    }
    // else {
    //     q = searchKeyData();
    //     if (q == NULL || *q == '\0') {
    //         /* FIXME: gettextize? */
    //         printf("(Download)Save file to: ");
    //         fflush(stdout);
    //         filen = Strfgets(stdin);
    //         if (filen->length == 0)
    //             return -1;
    //         q = filen->ptr;
    //     }
    //     for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
    //         ;
    //     *(p + 1) = '\0';
    //     if (*q == '\0')
    //         return -1;
    //     p = q;
    //     if (*p == '|' && PermitSaveToPipe)
    //         is_pipe = TRUE;
    //     else {
    //         p = expandPath(p);
    //         if (checkOverWrite(p) < 0)
    //             return -1;
    //     }
    //     if (checkCopyFile(tmpf, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't copy. %s and %s are identical.", tmpf, p);
    //         return -1;
    //     }
    //     if (_MoveFile(tmpf, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't save to %s\n", p);
    //         return -1;
    //     }
    //     if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
    //         setModtime(p, st.st_mtime);
    // }
    return 0;
}

int doFileMove(char* tmpf, char* defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
}

int doFileSave(struct URLFile uf, char* defstr)
{
    Str msg;
    Str filen;
    char *p, *q;
    pid_t pid;
    char* lock;
    char* tmpf = NULL;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
    FILE* f;
#endif

    // if (fmInitialized)
    {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            p = inputLineHist(getUI(), "(Download)Save file to: ",
                defstr, IN_FILENAME, SaveHist);
            if (p == NULL || *p == '\0')
                return -1;
            p = conv_to_system(p);
        }
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(uf.stream, p) < 0) {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't save. Load file and %s are identical.",
                conv_from_system(p));
            message(getUI(), MSG_ERR, msg->ptr);
            return -1;
        }
        /*
         * if (save2tmp(uf, p) < 0) {
         * msg = Sprintf("Can't save to %s", conv_from_system(p));
         * message(getUI(), MSG_ERR, msg->ptr);
         * }
         */
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
        symlink(p, lock);
#else
        f = fopen(lock, "w");
        if (f)
            fclose(f);
#endif
        flush_tty();
        pid = fork();
        if (!pid) {
            int err;
            if ((uf.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
                uncompress_stream(&uf, &tmpf);
                if (tmpf)
                    unlink(tmpf);
            }
            setup_child(FALSE, 0, UFfileno(&uf));
            err = save2tmp(uf, p);
            if (err == 0 && PreserveTimestamp && uf.modtime != -1)
                setModtime(p, uf.modtime);
            UFclose(&uf);
            unlink(lock);
            if (err != 0)
                exit(-err);
            exit(0);
        }
        addDownloadList(pid, uf.url, p, lock, current_content_length);
    }
    // else {
    //     q = searchKeyData();
    //     if (q == NULL || *q == '\0') {
    //         /* FIXME: gettextize? */
    //         printf("(Download)Save file to: ");
    //         fflush(stdout);
    //         filen = Strfgets(stdin);
    //         if (filen->length == 0)
    //             return -1;
    //         q = filen->ptr;
    //     }
    //     for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
    //         ;
    //     *(p + 1) = '\0';
    //     if (*q == '\0')
    //         return -1;
    //     p = expandPath(q);
    //     if (checkOverWrite(p) < 0)
    //         return -1;
    //     if (checkSaveFile(uf.stream, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't save. Load file and %s are identical.", p);
    //         return -1;
    //     }
    //     if (uf.content_encoding != CMP_NOCOMPRESS && AutoUncompress) {
    //         uncompress_stream(&uf, &tmpf);
    //         if (tmpf)
    //             unlink(tmpf);
    //     }
    //     if (save2tmp(uf, p) < 0) {
    //         /* FIXME: gettextize? */
    //         printf("Can't save to %s\n", p);
    //         return -1;
    //     }
    //     if (PreserveTimestamp && uf.modtime != -1)
    //         setModtime(p, uf.modtime);
    // }
    return 0;
}

int checkCopyFile(char* path1, char* path2)
{
    struct stat st1, st2;

    if (*path2 == '|' && PermitSaveToPipe)
        return 0;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkSaveFile(InputStream stream, char* path2)
{
    struct stat st1, st2;
    int des = ISfileno(stream);

    if (des < 0)
        return 0;
    if (*path2 == '|' && PermitSaveToPipe)
        return 0;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkOverWrite(char* path)
{
    struct stat st;
    char* ans;

    if (stat(path, &st) < 0)
        return 0;
    /* FIXME: gettextize? */
    ans = inputAnswer("File exists. Overwrite? (y/n)");
    if (ans && TOLOWER(*ans) == 'y')
        return 0;
    else
        return -1;
}

static FILE*
lessopen_stream(char* path)
{
    char* lessopen;
    FILE* fp;
    Str tmpf;
    int c, n = 0;

    lessopen = getenv("LESSOPEN");
    if (lessopen == NULL || lessopen[0] == '\0')
        return NULL;

    if (lessopen[0] != '|') /* filename mode, not supported m(__)m */
        return NULL;

    /* pipe mode */
    ++lessopen;

    /* LESSOPEN must contain one conversion specifier for strings ('%s'). */
    for (const char* f = lessopen; *f; f++) {
        if (*f == '%') {
            if (f[1] == '%') /* Literal % */
                f++;
            else if (*++f == 's') {
                if (n)
                    return NULL;
                n++;
            } else
                return NULL;
        }
    }
    if (!n)
        return NULL;

    tmpf = Sprintf(lessopen, shell_quote(path));
    fp = popen(tmpf->ptr, "r");
    if (fp == NULL) {
        return NULL;
    }
    c = getc(fp);
    if (c == EOF) {
        pclose(fp);
        return NULL;
    }
    ungetc(c, fp);
    return fp;
}

static char*
guess_filename(char* file)
{
    char *p = NULL, *s;

    if (file != NULL)
        p = mybasename(file);
    if (p == NULL || *p == '\0')
        return DEF_SAVE_FILE;
    s = p;
    if (*p == '#')
        p++;
    while (*p != '\0') {
        if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
            *p = '\0';
            break;
        }
        p++;
    }
    return s;
}

char* guess_save_name(Buffer* buf, char* path)
{
    if (buf && buf->document_header) {
        Str name = NULL;
        char *p, *q;
        if ((p = getHttpHeaderValue(buf->document_header, "Content-Disposition:")) != NULL && (q = strcasestr(p, "filename")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "filename", 8, &name))
            path = name->ptr;
        else if ((p = getHttpHeaderValue(buf->document_header, "Content-Type:")) != NULL && (q = strcasestr(p, "name")) != NULL && (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') && matchattr(q, "name", 4, &name))
            path = name->ptr;
    }
    return guess_filename(path);
}

/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
