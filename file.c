#include "file.h"
#include "frame.h"
#include "html_builder.h"
#include "indep.h"
#include "alloc.h"
#include "mimehead.h"
#include "ftp.h"
#include "compression.h"
#include "mailcap.h"
#include "readbuffer.h"
#include "symbol.h"
#include "message.h"
#include "w3m_rc.h"
#include <libwc/conv.h>
#include "linein.h"
#include "ctrlcode.h"
#include "html_form.h"
#include "siteconf.h"
#include "http_request.h"
#include "buffer.h"
#include "anchor.h"
#include "maparea.h"
#include "download.h"
#include "tab.h"
#include "etc.h"
#include "image.h"
#include "fm.h"
#include "html_table.h"
#include "display.h"
#include "html.h"
#include "parsetagx.h"
#include "local_cgi.h"
#include "regex.h"
#include "myctype.h"

#include <libwc/ces.h>

#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <assert.h>

#include <libwc/charset.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */

static int frame_source = 0;
static int need_number = 0;

static JMP_BUF AbortLoading;
static MySignalHandler KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

struct link_stack {
    int cmd;
    short offset;
    short pos;
    struct link_stack* next;
};

static struct link_stack* link_stack = NULL;

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

#define INITIAL_FORM_SIZE 10
static int cur_form_id(struct HtmlBuilder* hb) { return ((hb->form_sp >= 0) ? hb->form_stack[hb->form_sp] : -1); }

#define MAX_UL_LEVEL 9
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)
#define IMG_SYMBOL UL_SYMBOL(12)
#define HR_SYMBOL 26

int currentLn(struct Buffer* buf)
{
    if (buf->doc.currentLine)
        /*     return buf->doc.currentLine->real_linenumber + 1;      */
        return buf->doc.currentLine->linenumber + 1;
    else
        return 1;
}

static struct Buffer*
loadSomething(struct Url url, struct input_stream* stream, const char* t,
    LoadBufferFunc loadproc, struct Buffer* defaultbuf, bool internal)
{
    struct Buffer* buf = loadproc(url, stream, t, defaultbuf, internal);
    if (!buf)
        return NULL;

    if (buf->buffername == NULL || buf->buffername[0] == '\0') {
        buf->buffername = checkHeader(&buf->content, "Subject:");
        if (buf->buffername == NULL && buf->content.filename != NULL)
            buf->buffername = conv_from_system(lastFileName(buf->content.filename));
    }
    if (buf->currentURL.scheme == SCM_UNKNOWN)
        buf->currentURL.scheme = url.scheme;
    // if (f->scheme == SCM_LOCAL && buf->sourcefile == NULL)
    //     buf->sourcefile = buf->content.filename;
    if (loadproc == loadHTMLBuffer || loadproc == loadImageBuffer)
        buf->type = "text/html";
    else
        buf->type = "text/plain";
    return buf;
}

int dir_exist(const char* path)
{
    struct stat stbuf;

    if (path == NULL || *path == '\0')
        return 0;
    if (stat(path, &stbuf) == -1)
        return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

static int
is_dump_text_type(const char* type)
{
    struct mailcap* mcap;
    return (type && (mcap = searchExtViewer(type)) && (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int
is_text_type(const char* type)
{
    return (type == NULL || type[0] == '\0' || strncasecmp(type, "text/", 5) == 0 || (strncasecmp(type, "application/", 12) == 0 && strstr(type, "xhtml") != NULL) || strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int
is_plain_text_type(const char* type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) || (is_text_type(type) && !is_dump_text_type(type)));
}

int is_html_type(const char* type)
{
    return (type && (strcasecmp(type, "text/html") == 0 || strcasecmp(type, "application/xhtml+xml") == 0));
}

static int
setModtime(const char* path, time_t modtime)
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

static FILE*
lessopen_stream(const char* path)
{
    const char* lessopen = getenv("LESSOPEN");
    if (!lessopen || lessopen[0] == '\0')
        return NULL;
    if (lessopen[0] != '|') /* content.filename mode, not supported m(__)m */
        return NULL;

    /* pipe mode */
    ++lessopen;

    /* LESSOPEN must contain one conversion specifier for strings ('%s'). */
    int n = 0;
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

    Str tmpf = Sprintf(lessopen, shell_quote(path));
    FILE* fp = popen(tmpf->ptr, "r");
    if (fp == NULL) {
        return NULL;
    }
    int c = getc(fp);
    if (c == EOF) {
        pclose(fp);
        return NULL;
    }
    ungetc(c, fp);
    return fp;
}

struct auth_param {
    char* name;
    Str val;
};

struct http_auth {
    int pri;
    char* scheme;
    struct auth_param* param;
    Str (*cred)(struct http_auth* ha, Str uname, Str pw, struct Url* pu,
        struct HttpRequest* hr, struct FormList* request);
};

enum {
    AUTHCHR_NUL,
    AUTHCHR_SEP,
    AUTHCHR_TOKEN,
};

static int
skip_auth_token(const char** pp)
{
    const char* p;
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
extract_auth_val(const char** q)
{
    const char* qq = *q;
    int quoted = 0;
    Str val = Strnew();

    qq = skip_blanks(qq);
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

static Str
qstr_unquote(Str s)
{
    char* p;

    if (s == NULL)
        return NULL;
    p = s->ptr;
    if (*p == '"') {
        Str tmp = Strnew();
        for (p++; *p != '\0'; p++) {
            if (*p == '\\')
                p++;
            Strcat_char(tmp, *p);
        }
        if (Strlastchar(tmp) == '"')
            Strshrink(tmp, 1);
        return tmp;
    } else
        return s;
}

static const char*
extract_auth_param(const char* q, struct auth_param* auth)
{
    struct auth_param* ap;
    const char* p;

    for (ap = auth; ap->name != NULL; ap++) {
        ap->val = NULL;
    }

    while (*q != '\0') {
        q = skip_blanks(q);
        for (ap = auth; ap->name != NULL; ap++) {
            size_t len;

            len = strlen(ap->name);
            if (strncasecmp(q, ap->name, len) == 0 && (IS_SPACE(q[len]) || q[len] == '=')) {
                p = q + len;
                p = skip_blanks(p);
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
                q = skip_blanks(q);
                if (*q != '=')
                    return p;
                q++;
                extract_auth_val(&q);
            } else
                return p;
        }
        if (*q != '\0') {
            q = skip_blanks(q);
            if (*q == ',')
                q++;
            else
                break;
        }
    }
    return q;
}

static Str
get_auth_param(struct auth_param* auth, char* name)
{
    struct auth_param* ap;
    for (ap = auth; ap->name != NULL; ap++) {
        if (strcasecmp(name, ap->name) == 0)
            return ap->val;
    }
    return NULL;
}

static Str
AuthBasicCred(struct http_auth* ha, Str uname, Str pw, struct Url* pu,
    struct HttpRequest* hr, struct FormList* request)
{
    Str s = Strdup(uname);
    Strcat_char(s, ':');
    Strcat(s, pw);
    return Strnew_m_charp("Basic ", base64_encode(s->ptr, s->length)->ptr, NULL);
}

#ifdef USE_DIGEST_AUTH
#include <openssl/md5.h>

/* RFC2617: 3.2.2 The Authorization Request Header
 *
 * credentials      = "Digest" digest-response
 * digest-response  = 1#( username | realm | nonce | digest-uri
 *                    | response | [ algorithm ] | [cnonce] |
 *                     [opaque] | [message-qop] |
 *                         [nonce-count]  | [auth-param] )
 *
 * username         = "username" "=" username-value
 * username-value   = quoted-string
 * digest-uri       = "uri" "=" digest-uri-value
 * digest-uri-value = request-uri   ; As specified by HTTP/1.1
 * message-qop      = "qop" "=" qop-value
 * cnonce           = "cnonce" "=" cnonce-value
 * cnonce-value     = nonce-value
 * nonce-count      = "nc" "=" nc-value
 * nc-value         = 8LHEX
 * response         = "response" "=" request-digest
 * request-digest = <"> 32LHEX <">
 * LHEX             =  "0" | "1" | "2" | "3" |
 *                     "4" | "5" | "6" | "7" |
 *                     "8" | "9" | "a" | "b" |
 *                     "c" | "d" | "e" | "f"
 */

static Str
digest_hex(unsigned char* p)
{
    char* h = "0123456789abcdef";
    Str tmp = Strnew_size(MD5_DIGEST_LENGTH * 2 + 1);
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++, p++) {
        Strcat_char(tmp, h[(*p >> 4) & 0x0f]);
        Strcat_char(tmp, h[*p & 0x0f]);
    }
    return tmp;
}

enum {
    QOP_NONE,
    QOP_AUTH,
    QOP_AUTH_INT,
};

static Str
AuthDigestCred(struct http_auth* ha, Str uname, Str pw, struct Url* pu,
    struct HttpRequest* hr, struct FormList* request)
{
    Str tmp, a1buf, a2buf, rd, s;
    unsigned char md5[MD5_DIGEST_LENGTH + 1];
    Str uri = HTTPrequestURI(pu, hr);
    char nc[] = "00000001";
    FILE* fp;

    Str algorithm = qstr_unquote(get_auth_param(ha->param, "algorithm"));
    Str nonce = qstr_unquote(get_auth_param(ha->param, "nonce"));
    Str cnonce /* = qstr_unquote(get_auth_param(ha->param, "cnonce")) */;
    /* cnonce is what client should generate. */
    Str qop = qstr_unquote(get_auth_param(ha->param, "qop"));

    static union {
        int r[4];
        unsigned char s[sizeof(int) * 4];
    } cnonce_seed;
    int qop_i = QOP_NONE;

    cnonce_seed.r[0] = rand();
    cnonce_seed.r[1] = rand();
    cnonce_seed.r[2] = rand();
    MD5(cnonce_seed.s, sizeof(cnonce_seed.s), md5);
    cnonce = digest_hex(md5);
    cnonce_seed.r[3]++;

    if (qop) {
        const char* p;
        size_t i;

        p = qop->ptr;
        p = skip_blanks(p);

        for (;;) {
            if ((i = strcspn(p, " \t,")) > 0) {
                if (i == sizeof("auth-int") - sizeof("") && !strncasecmp(p, "auth-int", i)) {
                    if (qop_i < QOP_AUTH_INT)
                        qop_i = QOP_AUTH_INT;
                } else if (i == sizeof("auth") - sizeof("") && !strncasecmp(p, "auth", i)) {
                    if (qop_i < QOP_AUTH)
                        qop_i = QOP_AUTH;
                }
            }

            if (p[i]) {
                p += i + 1;
                p = skip_blanks(p);
            } else
                break;
        }
    }

    /* A1 = unq(username-value) ":" unq(realm-value) ":" passwd */
    tmp = Strnew_m_charp(uname->ptr, ":",
        qstr_unquote(get_auth_param(ha->param, "realm"))->ptr,
        ":", pw->ptr, NULL);
    MD5((unsigned char*)tmp->ptr, strlen(tmp->ptr), md5);
    a1buf = digest_hex(md5);

    if (algorithm) {
        if (strcasecmp(algorithm->ptr, "MD5-sess") == 0) {
            /* A1 = H(unq(username-value) ":" unq(realm-value) ":" passwd)
             *      ":" unq(nonce-value) ":" unq(cnonce-value)
             */
            if (nonce == NULL)
                return NULL;
            tmp = Strnew_m_charp(a1buf->ptr, ":",
                qstr_unquote(nonce)->ptr,
                ":", qstr_unquote(cnonce)->ptr, NULL);
            MD5((unsigned char*)tmp->ptr, strlen(tmp->ptr), md5);
            a1buf = digest_hex(md5);
        } else if (strcasecmp(algorithm->ptr, "MD5") == 0)
            /* ok default */
            ;
        else
            /* unknown algorithm */
            return NULL;
    }

    /* A2 = Method ":" digest-uri-value */
    tmp = Strnew_m_charp(HTTPrequestMethod(hr)->ptr, ":", uri->ptr, NULL);
    if (qop_i == QOP_AUTH_INT) {
        /*  A2 = Method ":" digest-uri-value ":" H(entity-body) */
        if (request && request->body) {
            if (request->method == FORM_METHOD_POST && request->enctype == FORM_ENCTYPE_MULTIPART) {
                fp = fopen(request->body, "r");
                if (fp != NULL) {
                    Str ebody;
                    ebody = Strfgetall(fp);
                    fclose(fp);
                    MD5((unsigned char*)ebody->ptr, strlen(ebody->ptr), md5);
                } else {
                    MD5((unsigned char*)"", 0, md5);
                }
            } else {
                MD5((unsigned char*)request->body, request->length, md5);
            }
        } else {
            MD5((unsigned char*)"", 0, md5);
        }
        Strcat_char(tmp, ':');
        Strcat(tmp, digest_hex(md5));
    }
    MD5((unsigned char*)tmp->ptr, strlen(tmp->ptr), md5);
    a2buf = digest_hex(md5);

    if (qop_i >= QOP_AUTH) {
        /* request-digest  = <"> < KD ( H(A1),     unq(nonce-value)
         *                      ":" nc-value
         *                      ":" unq(cnonce-value)
         *                      ":" unq(qop-value)
         *                      ":" H(A2)
         *                      ) <">
         */
        if (nonce == NULL)
            return NULL;
        tmp = Strnew_m_charp(a1buf->ptr, ":", qstr_unquote(nonce)->ptr,
            ":", nc,
            ":", qstr_unquote(cnonce)->ptr,
            ":", qop_i == QOP_AUTH ? "auth" : "auth-int",
            ":", a2buf->ptr, NULL);
        MD5((unsigned char*)tmp->ptr, strlen(tmp->ptr), md5);
        rd = digest_hex(md5);
    } else {
        /* compatibility with RFC 2069
         * request_digest = KD(H(A1),  unq(nonce), H(A2))
         */
        tmp = Strnew_m_charp(a1buf->ptr, ":",
            qstr_unquote(get_auth_param(ha->param, "nonce"))->ptr, ":", a2buf->ptr, NULL);
        MD5((unsigned char*)tmp->ptr, strlen(tmp->ptr), md5);
        rd = digest_hex(md5);
    }

    /*
     * digest-response  = 1#( username | realm | nonce | digest-uri
     *                          | response | [ algorithm ] | [cnonce] |
     *                          [opaque] | [message-qop] |
     *                          [nonce-count]  | [auth-param] )
     */

    tmp = Strnew_m_charp("Digest username=\"", uname->ptr, "\"", NULL);
    if ((s = get_auth_param(ha->param, "realm")) != NULL)
        Strcat_m_charp(tmp, ", realm=", s->ptr, NULL);
    if ((s = get_auth_param(ha->param, "nonce")) != NULL)
        Strcat_m_charp(tmp, ", nonce=", s->ptr, NULL);
    Strcat_m_charp(tmp, ", uri=\"", uri->ptr, "\"", NULL);
    Strcat_m_charp(tmp, ", response=\"", rd->ptr, "\"", NULL);

    if (algorithm && (s = get_auth_param(ha->param, "algorithm")))
        Strcat_m_charp(tmp, ", algorithm=", s->ptr, NULL);

    if (cnonce)
        Strcat_m_charp(tmp, ", cnonce=\"", cnonce->ptr, "\"", NULL);

    if ((s = get_auth_param(ha->param, "opaque")) != NULL)
        Strcat_m_charp(tmp, ", opaque=", s->ptr, NULL);

    if (qop_i >= QOP_AUTH) {
        Strcat_m_charp(tmp, ", qop=",
            qop_i == QOP_AUTH ? "auth" : "auth-int",
            NULL);
        /* XXX how to count? */
        /* Since nonce is unique up to each *-Authenticate and w3m does not re-use *-Authenticate: headers,
           nonce-count should be always "00000001". */
        Strcat_m_charp(tmp, ", nc=", nc, NULL);
    }

    return tmp;
}
#endif

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
findAuthentication(struct http_auth* hauth, struct Buffer* buf, char* auth_field)
{
    struct http_auth* ha;
    int len = strlen(auth_field), slen;
    TextListItem* i;
    const char *p0, *p;

    bzero(hauth, sizeof(struct http_auth));
    for (i = buf->content.document_header->first; i != NULL; i = i->next) {
        if (strncasecmp(i->ptr, auth_field, len) == 0) {
            for (p = i->ptr + len; p != NULL && *p != '\0';) {
                p = skip_blanks(p);
                p0 = p;
                for (ha = &www_auth[0]; ha->scheme != NULL; ha++) {
                    slen = strlen(ha->scheme);
                    if (strncasecmp(p, ha->scheme, slen) == 0) {
                        p += slen;
                        p = skip_blanks(p);
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
                        p = skip_blanks(p);
                        p = extract_auth_param(p, none_auth_param);
                    } else
                        break;
                }
            }
        }
    }
    return hauth->scheme ? hauth : NULL;
}

static void
getAuthCookie(struct http_auth* hauth, char* auth_header,
    struct TextList* extra_header, struct Url* pu, struct HttpRequest* hr,
    struct FormList* request,
    volatile Str* uname, volatile Str* pwd)
{
    char* realm = NULL;
    if (hauth)
        realm = qstr_unquote(get_auth_param(hauth->param, "realm"))->ptr;

    if (!realm)
        return;

    int auth_header_len = strlen(auth_header);
    bool a_found = FALSE;
    TextListItem* i;
    for (i = extra_header->first; i != NULL; i = i->next) {
        if (!strncasecmp(i->ptr, auth_header, auth_header_len)) {
            a_found = TRUE;
            break;
        }
    }
    int proxy = !strncasecmp("Proxy-Authorization:", auth_header,
        auth_header_len);
    if (a_found) {
        /* This means that *-Authenticate: header is received after
         * Authorization: header is sent to the server.
         */
        if (fmInitialized()) {
            message("Wrong username or password", 0, 0);
        } else
            fprintf(stderr, "Wrong username or password\n");
        sleep(1);
        /* delete Authenticate: header from extra_header */
        delText(extra_header, i);
        invalidate_auth_user_passwd(pu, realm, *uname, *pwd, proxy);
    }
    *uname = NULL;
    *pwd = NULL;

    if (!a_found && find_auth_user_passwd(pu, realm, (Str*)uname, (Str*)pwd, proxy)) {
        /* found username & password in passwd file */;
    } else {
        if (getRuntime()->QuietMessage)
            return;
        /* input username and password */
        sleep(2);
        if (fmInitialized()) {
            char* pp;
            enterRawMode();
            /* FIXME: gettextize? */
            if ((pp = inputStr(Sprintf("Username for %s: ", realm)->ptr,
                     NULL))
                == NULL)
                return;
            *uname = Str_conv_to_system(Strnew_charp(pp));
            if ((pp = inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
                     IN_PASSWORD))
                == NULL) {
                *uname = NULL;
                return;
            }
            *pwd = Str_conv_to_system(Strnew_charp(pp));
            exitRawMode();
        } else {
            /*
             * If post file is specified as '-', stdin is closed at this
             * point.
             * In this case, w3m cannot read username from stdin.
             * So exit with error message.
             * (This is same behavior as lwp-request.)
             */
            if (feof(stdin) || ferror(stdin)) {
                /* FIXME: gettextize? */
                fprintf(stderr, "w3m: Authorization required for %s\n",
                    realm);
                exit(1);
            }

            /* FIXME: gettextize? */
            printf(proxy ? "Proxy Username for %s: " : "Username for %s: ",
                realm);
            fflush(stdout);
            *uname = Strfgets(stdin);
            Strchop(*uname);

            *pwd = Strnew_charp((char*)
                    getpass(proxy ? "Proxy Password: " : "Password: "));
        }
    }

    Str ss = hauth->cred(hauth, *uname, *pwd, pu, hr, request);
    if (ss) {
        Str tmp = Strnew_charp(auth_header);
        Strcat_m_charp(tmp, " ", ss->ptr, "\r\n", NULL);
        pushText(extra_header, tmp->ptr);
    } else {
        *uname = NULL;
        *pwd = NULL;
    }
    return;
}

static int
same_url_p(struct Url* pu1, struct Url* pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port && (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1)
        && (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

static int
checkRedirection(struct Url* pu)
{
    static struct Url* puv = NULL;
    static int nredir = 0;
    static int nredir_size = 0;
    Str tmp;

    if (pu == NULL) {
        nredir = 0;
        nredir_size = 0;
        puv = NULL;
        return TRUE;
    }
    if (nredir >= getRuntime()->FollowRedirection) {
        /* FIXME: gettextize? */
        tmp = Sprintf("Number of redirections exceeded %d at %s",
            getRuntime()->FollowRedirection, parsedURL2Str(pu)->ptr);
        disp_err_message(tmp->ptr, FALSE);
        return FALSE;
    } else if (nredir_size > 0 && (same_url_p(pu, &puv[(nredir - 1) % nredir_size]) || (!(nredir % 2) && same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
        /* FIXME: gettextize? */
        tmp = Sprintf("Redirection loop detected (%s)",
            parsedURL2Str(pu)->ptr);
        disp_err_message(tmp->ptr, FALSE);
        return FALSE;
    }
    if (!puv) {
        nredir_size = getRuntime()->FollowRedirection / 2 + 1;
        puv = New_N(struct Url, nredir_size);
        memset(puv, 0, sizeof(struct Url) * nredir_size);
    }
    copyParsedURL(&puv[nredir % nredir_size], pu);
    nredir++;
    return TRUE;
}

Str getLinkNumberStr(struct HtmlBuilder* hb, int correction)
{
    return Sprintf("[%d]", hb->cur_hseq + correction);
}

/*
 * loadGeneralFile: load file to buffer
 */

struct Buffer* page_loaded(struct Url url,
    wc_ces charset, Str page, bool do_download)
{
    assert(page);

    if (getRuntime()->image_source)
        return NULL;

    Str tmp = tmpfname(TMPF_SRC, ".html");
    FILE* src = fopen(tmp->ptr, "w");
    if (src) {
        Str s = wc_Str_conv_strict(page, getRuntime()->InnerCharset, charset);
        Strfputs(s, src);
        fclose(src);
    }

    if (do_download) {
        if (!src)
            return NULL;
        const char* file = guess_filename(url.file);
        doFileMove(tmp->ptr, file);
        return NO_BUFFER;
    }

    struct Buffer* b = loadHTMLString(page);
    if (b) {
        copyParsedURL(&b->currentURL, &url);
        if (src)
            b->sourcefile = tmp->ptr;

        b->document_charset = charset;
    }
    return b;
}

static struct Buffer* make_buffer(struct Url url, int flag,
    struct input_stream* stream, struct Buffer* t_buf, const char* t,
    const char* ssl_certificate,
    bool do_download)
{
    LoadBufferFunc proc = loadBuffer;

    MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;
    if (do_download) {
        /* download only */
        const char* file;
        TRAP_OFF;
        if (url.scheme == SCM_LOCAL) {
            // struct stat st;
            // if (getRuntime()->PreserveTimestamp && !stat(url.real_file, &st)) {
            //     f.modtime = st.st_mtime;
            // }
            file = conv_from_system(guess_save_name(NULL, url.real_file));
        } else {
            file = guess_save_name(&t_buf->content, url.file);
        }
        if (doFileSave(url, stream, file, t_buf->content.compression) == 0)
            UFhalfclose(stream, url.scheme);
        else
            is_close(stream);
        return NO_BUFFER;
    }

    if (t_buf) {
        bool use_tmpf = url.scheme != SCM_LOCAL && !getRuntime()->image_source;
        if ((t_buf->content.compression != CMP_NOCOMPRESS) && getRuntime()->AutoUncompress
            && !(getRuntime()->w3m_dump & DUMP_EXTRA)) {
            stream = uncompress_stream(stream,
                t_buf->content.compression, use_tmpf ? &url.real_file : NULL);
            // UFhalfclose(&f);
        } else if (t_buf->content.compression != CMP_NOCOMPRESS) {
            if (!(getRuntime()->w3m_dump & DUMP_SOURCE) && (getRuntime()->w3m_dump & ~DUMP_FRAME || is_text_type(t) || searchExtViewer(t))) {
                stream = uncompress_stream(stream,
                    t_buf->content.compression, use_tmpf ? &t_buf->sourcefile : NULL);
                // UFhalfclose(&f);
                // const char* ext;
                // uncompressed_file_type(url.file, &ext);
            } else {
                t = compress_application_type(t_buf->content.compression);
                // f.compression = CMP_NOCOMPRESS;
            }
        }
    }

    if (getRuntime()->image_source) {
        struct Buffer* b = NULL;
        if (is_save2tmp(stream, getRuntime()->image_source)) {
            b = newBuffer(INIT_BUFFER_WIDTH);
            b->sourcefile = getRuntime()->image_source;
        }
        is_close(stream);
        TRAP_OFF;
        return b;
    }

    if (is_html_type(t))
        proc = loadHTMLBuffer;
    else if (is_plain_text_type(t))
        proc = loadBuffer;
    else if (getRuntime()->activeImage && getRuntime()->displayImage && !getRuntime()->useExtImageViewer && !(getRuntime()->w3m_dump & ~DUMP_FRAME) && !strncasecmp(t, "image/", 6))
        proc = loadImageBuffer;
    else if (getRuntime()->w3m_backend)
        ;
    else if (!(getRuntime()->w3m_dump & ~DUMP_FRAME) || is_dump_text_type(t)) {
        if (!do_download && searchExtViewer(t) != NULL) {
            proc = doExternal;
        } else {
            TRAP_OFF;
            if (url.scheme == SCM_LOCAL) {
                is_close(stream);
                _doFileCopy(url.real_file,
                    conv_from_system(guess_save_name(NULL, url.real_file)), TRUE);
            } else {
                if (doFileSave(url, stream,
                        guess_save_name(&t_buf->content, url.file),
                        t_buf->content.compression)
                    == 0)
                    UFhalfclose(stream, url.scheme);
                else
                    is_close(stream);
            }
            return NO_BUFFER;
        }
    } else if (getRuntime()->w3m_dump & DUMP_FRAME)
        return NULL;

    if (t_buf == NULL)
        t_buf = newBuffer(INIT_BUFFER_WIDTH);
    copyParsedURL(&t_buf->currentURL, &url);
    t_buf->content.filename = url.real_file ? url.real_file : url.file ? conv_to_system(url.file)
                                                                       : NULL;
    if (flag & RG_FRAME) {
        t_buf->bufferprop |= BP_FRAME;
    }
    t_buf->ssl_certificate = ssl_certificate;
    frame_source = flag & RG_FRAME_SRC;

    struct Buffer* b = loadSomething(url, stream, t,
        proc, t_buf, t_buf->bufferprop & BP_FRAME);
    is_close(stream);
    frame_source = 0;
    if (b && b != NO_BUFFER) {
        if (getRuntime()->w3m_backend)
            b->type = allocStr(t, -1);
        if (url.label) {
            if (proc == loadHTMLBuffer) {
                struct Anchor* a;
                a = searchURLLabel(b, url.label);
                if (a != NULL) {
                    gotoLine(b, a->start.line);
                    if (getRuntime()->label_topline)
                        b->doc.topLine = lineSkip(b, b->doc.topLine,
                            b->doc.currentLine->linenumber
                                - b->doc.topLine->linenumber,
                            FALSE);
                    b->pos = a->start.pos;
                    arrangeCursor(b);
                }
            } else { /* plain text */
                int l = atoi(url.label);
                gotoRealLine(b, l);
                b->pos = 0;
                arrangeCursor(b);
            }
        }
    }
    if (getRuntime()->header_string)
        getRuntime()->header_string = NULL;
    if (b && b != NO_BUFFER)
        preFormUpdateBuffer(b);
    TRAP_OFF;
    return b;
}

MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

struct AuthInfo {
    bool add_auth_cookie_flag;
    Str uname;
    Str pwd;
    Str realm;
};

struct Buffer* load_doc(const char* path, struct Url* current,
    struct FormList* request,
    struct URLOption option,
    struct AuthInfo auth,
    bool do_download,
    struct Buffer* t_buf,
    struct input_stream* connection)
{
    //
    // siteconf redirection
    //
    {
        struct Url pu;
        parseURL2(path, &pu, current);
        const char* sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
        if (sc_redirect && *sc_redirect && checkRedirection(&pu)) {
            struct Url* new_current = New(struct Url);
            *new_current = pu;
            return load_doc(sc_redirect, new_current,
                NULL, option, auth, do_download, t_buf, NULL);
        }
    }

    TRAP_OFF;

    unsigned char status = HTST_NORMAL;
    struct HttpRequest hr;
    struct UrlStream us = openURL(path, current, request,
        (struct URLOption) {}, connection, do_download);
    if (!us.stream && getRuntime()->retryAsHttp && us.url_str[0] != '/') {
        if (us.url.scheme == SCM_MISSING || us.url.scheme == SCM_UNKNOWN) {
            // retry it as "http://"
            const char* u = Strnew_m_charp("http://", path, NULL)->ptr;
            us = openURL(u, current, request,
                (struct URLOption) {}, connection, do_download);
        }
    }

    if (!us.stream) {
        // non stream(file or socket) content.
        wc_ces charset = WC_CES_US_ASCII;
        Str page = NULL;
        switch (us.url.scheme) {
        case SCM_LOCAL: {
            struct stat st;
            if (stat(us.url.real_file, &st) < 0)
                return NULL;
            if (S_ISDIR(st.st_mode)) {
                if (getRuntime()->UseExternalDirBuffer) {
                    Str cmd = Sprintf("%s?dir=%s#current",
                        getRuntime()->DirBufferCommand, us.url.file);
                    struct Buffer* b = loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0,
                        NULL, do_download);
                    if (b != NULL && b != NO_BUFFER) {
                        copyParsedURL(&b->currentURL, &us.url);
                        b->content.filename = b->currentURL.real_file;
                    }
                    return b;
                } else {
                    page = loadLocalDir(us.url.real_file);
                    charset = getRuntime()->SystemCharset;
                }
            }
        } break;
        case SCM_FTPDIR:
            page = loadFTPDir(&us.url, &charset, do_download);
            break;

        case SCM_UNKNOWN: {
            Str tmp = searchURIMethods(&us.url);
            if (tmp != NULL) {
                struct Buffer* b = loadGeneralFile(tmp->ptr, current,
                    option.referer, option.flag, request, do_download);
                if (b != NULL && b != NO_BUFFER)
                    copyParsedURL(&b->currentURL, &us.url);
                return b;
            }

            disp_err_message(Sprintf("Unknown URI: %s",
                                 parsedURL2Str(&us.url)->ptr)
                                 ->ptr,
                FALSE);
            break;
        }

        default:
            break;
        }

        if (page && page->length > 0)
            return page_loaded(us.url, charset, page, do_download);

        return NULL;
    }

    if (status == HTST_MISSING) {
        TRAP_OFF;
        is_close(us.stream);
        return NULL;
    }

    // openURL() succeeded
    if (SETJMP(AbortLoading) != 0) {
        /* transfer interrupted */
        TRAP_OFF;
        // if (b)
        //     discardBuffer(b);
        is_close(us.stream);
        return NULL;
    }

    if (us.is_cgi) {
        /* local CGI */
        // searchHeader = TRUE;
        // searchHeader_through = FALSE;
    }
    if (getRuntime()->header_string)
        getRuntime()->header_string = NULL;

    const char* t = "text/plain";

    TRAP_ON;
    if (us.url.scheme == SCM_HTTP || us.url.scheme == SCM_HTTPS || (((us.url.scheme == SCM_FTP && non_null(getRuntime()->FTP_proxy))) && getRuntime()->use_proxy && !check_no_proxy(us.url.host))) {

        if (fmInitialized()) {
            exitRawMode();
            /* FIXME: gettextize? */
            message(Sprintf("%s contacted. Waiting for reply...", us.url.host)->ptr, 0, 0);
        }
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
        getHttpResponseHeader(&t_buf->content, us.url, us.stream);
        const char* p;
        if (((t_buf->content.http_response_code >= 301 //
                 && t_buf->content.http_response_code <= 303)
                || t_buf->content.http_response_code == 307)
            && (p = checkHeader(&t_buf->content, "Location:")) != NULL
            && checkRedirection(&us.url)) {
            // document moved
            // 301: Moved Permanently
            // 302: Found
            // 303: See Other
            // 307: Temporary Redirect (HTTP/1.1)
            const char* tpath = url_encode(p, NULL, 0);
            is_close(us.stream);
            struct Url* new_current = New(struct Url);
            copyParsedURL(new_current, &us.url);
            struct Buffer* t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->bufferprop |= BP_REDIRECTED;
            return load_doc(tpath, new_current,
                NULL, option, auth, do_download, t_buf, NULL);
        }

        t = checkContentType(&t_buf->content);
        if (t == NULL && us.url.file != NULL) {
            if (!((t_buf->content.http_response_code >= 400 //
                      && t_buf->content.http_response_code <= 407) //
                    || (t_buf->content.http_response_code >= 500 //
                        && t_buf->content.http_response_code <= 505)))
                t = guessContentType(us.url.file);
        }
        if (t == NULL)
            t = "text/plain";
        if (auth.add_auth_cookie_flag
            && auth.realm && auth.uname && auth.pwd) {
            /* If authorization is required and passed */
            add_auth_user_passwd(&us.url, qstr_unquote(auth.realm)->ptr,
                auth.uname,
                auth.pwd,
                0);
            auth.add_auth_cookie_flag = 0;
        }
        if ((p = checkHeader(&t_buf->content, "WWW-Authenticate:")) != NULL //
            && t_buf->content.http_response_code == 401) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL
                && (auth.realm = get_auth_param(hauth.param, "realm")) != NULL) {
                struct Url* auth_pu = &us.url;
                getAuthCookie(&hauth, "Authorization:", option.extra_header,
                    auth_pu, &hr, request, &auth.uname, &auth.pwd);
                if (auth.uname == NULL) {
                    /* abort */
                    TRAP_OFF;
                    return make_buffer(us.url, option.flag,
                        us.stream, t_buf, t, us.ssl_certificate, do_download);
                }
                is_close(us.stream);
                auth.add_auth_cookie_flag = 1;
                return load_doc(path, current,
                    request, option, auth, do_download, t_buf, connection);
            }
        }
        if ((p = checkHeader(&t_buf->content, "Proxy-Authenticate:")) != NULL //
            && t_buf->content.http_response_code == 407) {
            // Authentication needed
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf, "Proxy-Authenticate:")
                    != NULL
                && (auth.realm = get_auth_param(hauth.param, "realm")) != NULL) {
                struct Url* auth_pu = schemeToProxy(us.url.scheme);
                getAuthCookie(&hauth, "Proxy-Authorization:",
                    option.extra_header, auth_pu, &hr, request,
                    &auth.uname, &auth.pwd);
                if (auth.uname == NULL) {
                    /* abort */
                    TRAP_OFF;
                    return make_buffer(us.url, option.flag,
                        us.stream, t_buf, t, us.ssl_certificate, do_download);
                }
                is_close(us.stream);
                auth.add_auth_cookie_flag = 1;
                add_auth_user_passwd(auth_pu,
                    qstr_unquote(auth.realm)->ptr, auth.uname, auth.pwd, 1);
                return load_doc(path, current,
                    request, option, auth, do_download, t_buf, connection);
            }
        }

        if (status == HTST_CONNECT) {
            // XXX: RFC2617 3.2.3 Authentication-Info: ?
            return load_doc(path, current,
                request, option, auth, do_download, t_buf, us.stream);
        }

        us.modtime = mymktime(checkHeader(&t_buf->content, "Last-Modified:"));
    } else if (us.url.scheme == SCM_FTP) {
        enum CompressionType compression = check_compression(path);
        if (compression != CMP_NOCOMPRESS) {
            t = uncompressed_file_type(us.url.file, NULL);
        } else {
            t = guessContentType(us.url.file);
        }
    } else if (us.is_cgi) {
        // searchHeader = SearchHeader = FALSE;
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
        getHttpResponseHeader(&t_buf->content, us.url, us.stream);
        const char* p;
        if ((p = checkHeader(&t_buf->content, "Location:")) != NULL && checkRedirection(&us.url)) {
            //
            // document moved
            //
            const char* tpath = url_encode(remove_space(p), NULL, 0);
            is_close(us.stream);
            auth.add_auth_cookie_flag = 0;
            struct Url* new_current = New(struct Url);
            copyParsedURL(new_current, &us.url);
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->bufferprop |= BP_REDIRECTED;
            return load_doc(tpath, new_current,
                NULL, option, auth, do_download, t_buf, NULL);
        }
        t = checkContentType(&t_buf->content);
        if (t == NULL)
            t = "text/plain";
    } else if (getRuntime()->DefaultType) {
        t = getRuntime()->DefaultType;
        getRuntime()->DefaultType = NULL;
    } else {
        t = guessContentType(us.url.file);
    }

    const char* p = checkHeader(t_buf ? &t_buf->content : NULL, "Content-Length:");
    if (p)
        t_buf->content.current_content_length = strtoclen(p);

    return make_buffer(us.url, option.flag, us.stream,
        t_buf, t, us.ssl_certificate, do_download);
}

struct Buffer*
loadGeneralFile(const char* path, struct Url* current, const char* referer,
    int flag, struct FormList* request, bool do_download)
{
    checkRedirection(NULL);
    prevtrap = NULL;
    return load_doc(path, current, request,
        (struct URLOption) {
            .flag = flag,
            .referer = referer,
            .extra_header = newTextList(),
        },
        (struct AuthInfo) {
            .realm = NULL,
            .uname = NULL,
            .pwd = NULL,
        },
        do_download, NULL, NULL);
}

#define TAG_IS(s, tag, len) \
    (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static char*
has_hidden_link(struct readbuffer* obuf, int cmd)
{
    Str line = obuf->line;
    struct link_stack* p;

    if (Strlastchar(line) != '>')
        return NULL;

    for (p = link_stack; p; p = p->next)
        if (p->cmd == cmd)
            break;
    if (!p)
        return NULL;

    if (obuf->pos == p->pos)
        return line->ptr + p->offset;

    return NULL;
}

static void
push_link(int cmd, int offset, int pos)
{
    struct link_stack* p;
    p = New(struct link_stack);
    p->cmd = cmd;
    p->offset = (short)offset;
    if (p->offset < 0)
        p->offset = 0;
    p->pos = (short)pos;
    if (p->pos < 0)
        p->pos = 0;
    p->next = link_stack;
    link_stack = p;
}

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
is_word_char(const unsigned char* ch)
{
    Lineprop ctype = get_mctype((const char*)ch);

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
    if (*ch == ' ' // NBSP_CODE
    )
        return 1;
    return 0;
}

static int is_combining_char(const unsigned char* ch)
{
    Lineprop ctype = get_mctype((const char*)ch);

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

static void
set_breakpoint(struct readbuffer* obuf, int tag_length)
{
    obuf->bp.len = obuf->line->length;
    obuf->bp.pos = obuf->pos;
    obuf->bp.tlen = tag_length;
    obuf->bp.flag = obuf->flag;
    obuf->bp.top_margin = obuf->top_margin;
    obuf->bp.bottom_margin = obuf->bottom_margin;

    if (!obuf->bp.init_flag)
        return;

    bcopy((void*)&obuf->anchor, (void*)&obuf->bp.anchor,
        sizeof(obuf->anchor));
    obuf->bp.img_alt = obuf->img_alt;
    obuf->bp.input_alt = obuf->input_alt;
    obuf->bp.in_bold = obuf->in_bold;
    obuf->bp.in_italic = obuf->in_italic;
    obuf->bp.in_under = obuf->in_under;
    obuf->bp.in_strike = obuf->in_strike;
    obuf->bp.in_ins = obuf->in_ins;
    obuf->bp.nobr_level = obuf->nobr_level;
    obuf->bp.prev_ctype = obuf->prev_ctype;
    obuf->bp.init_flag = 0;
}

static void
back_to_breakpoint(struct readbuffer* obuf)
{
    obuf->flag = obuf->bp.flag;
    bcopy((void*)&obuf->bp.anchor, (void*)&obuf->anchor,
        sizeof(obuf->anchor));
    obuf->img_alt = obuf->bp.img_alt;
    obuf->input_alt = obuf->bp.input_alt;
    obuf->in_bold = obuf->bp.in_bold;
    obuf->in_italic = obuf->bp.in_italic;
    obuf->in_under = obuf->bp.in_under;
    obuf->in_strike = obuf->bp.in_strike;
    obuf->in_ins = obuf->bp.in_ins;
    obuf->prev_ctype = obuf->bp.prev_ctype;
    obuf->pos = obuf->bp.pos;
    obuf->top_margin = obuf->bp.top_margin;
    obuf->bottom_margin = obuf->bp.bottom_margin;
    if (obuf->flag & RB_NOBR)
        obuf->nobr_level = obuf->bp.nobr_level;
}

static void
append_tags(struct readbuffer* obuf)
{
    int i;
    int len = obuf->line->length;
    int set_bp = 0;

    for (i = 0; i < obuf->tag_sp; i++) {
        switch (obuf->tag_stack[i]->cmd) {
        case HTML_A:
        case HTML_IMG_ALT:
        case HTML_B:
        case HTML_U:
        case HTML_I:
        case HTML_S:
            push_link(obuf->tag_stack[i]->cmd, obuf->line->length, obuf->pos);
            break;
        }
        Strcat_charp(obuf->line, obuf->tag_stack[i]->cmdname);
        switch (obuf->tag_stack[i]->cmd) {
        case HTML_NOBR:
            if (obuf->nobr_level > 1)
                break;
        case HTML_WBR:
            set_bp = 1;
            break;
        }
    }
    obuf->tag_sp = 0;
    if (set_bp)
        set_breakpoint(obuf, obuf->line->length - len);
}

static void
push_tag(struct readbuffer* obuf, char* cmdname, int cmd)
{
    obuf->tag_stack[obuf->tag_sp] = New(struct cmdtable);
    obuf->tag_stack[obuf->tag_sp]->cmdname = allocStr(cmdname, -1);
    obuf->tag_stack[obuf->tag_sp]->cmd = cmd;
    obuf->tag_sp++;
    if (obuf->tag_sp >= TAG_STACK_SIZE || obuf->flag & (RB_SPECIAL & ~RB_NOBR))
        append_tags(obuf);
}

static void
push_nchars(struct readbuffer* obuf, int width,
    const char* str, int len, Lineprop mode)
{
    append_tags(obuf);
    Strcat_charp_n(obuf->line, str, len);
    obuf->pos += width;
    if (width > 0) {
        Strcopy_charp_n(obuf->prevchar, str, len);
        obuf->prev_ctype = mode;
    }
    obuf->flag |= RB_NFLUSHED;
}

#define push_charp(obuf, width, str, mode) \
    push_nchars(obuf, width, str, strlen(str), mode)

#define push_str(obuf, width, str, mode) \
    push_nchars(obuf, width, str->ptr, str->length, mode)

static void
check_breakpoint(struct readbuffer* obuf, int pre_mode, const char* ch)
{
    int tlen, len = obuf->line->length;

    append_tags(obuf);
    if (pre_mode)
        return;
    tlen = obuf->line->length - len;
    if (tlen > 0
        || is_boundary((unsigned char*)obuf->prevchar->ptr,
            (unsigned char*)ch))
        set_breakpoint(obuf, tlen);
}

static void
push_char(struct readbuffer* obuf, int pre_mode, char ch)
{
    check_breakpoint(obuf, pre_mode, &ch);
    Strcat_char(obuf->line, ch);
    obuf->pos++;
    Strcopy_charp_n(obuf->prevchar, &ch, 1);
    if (ch != ' ')
        obuf->prev_ctype = PC_ASCII;
    obuf->flag |= RB_NFLUSHED;
}

#define PUSH(c) push_char(obuf, obuf->flag& RB_SPECIAL, c)

static void
push_spaces(struct readbuffer* obuf, int pre_mode, int width)
{
    int i;

    if (width <= 0)
        return;
    check_breakpoint(obuf, pre_mode, " ");
    for (i = 0; i < width; i++)
        Strcat_char(obuf->line, ' ');
    obuf->pos += width;
    Strcopy_charp_n(obuf->prevchar, " ", 1);
    obuf->flag |= RB_NFLUSHED;
}

static void
proc_mchar(struct readbuffer* obuf, int pre_mode,
    int width, const char** str, Lineprop mode)
{
    check_breakpoint(obuf, pre_mode, *str);
    obuf->pos += width;
    Strcat_charp_n(obuf->line, *str, get_mclen(*str));
    if (width > 0) {
        Strcopy_charp_n(obuf->prevchar, *str, 1);
        if (**str != ' ')
            obuf->prev_ctype = mode;
    }
    (*str) += get_mclen(*str);
    obuf->flag |= RB_NFLUSHED;
}

void push_render_image(Str str, int width, int limit,
    struct html_feed_environ* h_env)
{
    struct readbuffer* obuf = h_env->obuf;
    int indent = h_env->envs[h_env->envc].indent;

    push_spaces(obuf, 1, (limit - width) / 2);
    push_str(obuf, width, str, PC_ASCII);
    push_spaces(obuf, 1, (limit - width + 1) / 2);
    if (width > 0)
        flushline(h_env, obuf, indent, 0, h_env->limit);
}

static int
sloppy_parse_line(char** str)
{
    if (**str == '<') {
        while (**str && **str != '>')
            (*str)++;
        if (**str == '>')
            (*str)++;
        return 1;
    } else {
        while (**str && **str != '<')
            (*str)++;
        return 0;
    }
}

static void
passthrough(struct readbuffer* obuf, char* str, int back)
{
    int cmd;
    Str tok = Strnew();
    char* str_bak;

    if (back) {
        Str str_save = Strnew_charp(str);
        Strshrink(obuf->line, obuf->line->ptr + obuf->line->length - str);
        str = str_save->ptr;
    }
    while (*str) {
        str_bak = str;
        if (sloppy_parse_line(&str)) {
            const char* q = str_bak;
            cmd = gethtmlcmd(&q);
            if (back) {
                struct link_stack* p;
                for (p = link_stack; p; p = p->next) {
                    if (p->cmd == cmd) {
                        link_stack = p->next;
                        break;
                    }
                }
                back = 0;
            } else {
                Strcat_charp_n(tok, str_bak, str - str_bak);
                push_tag(obuf, tok->ptr, cmd);
                Strclear(tok);
            }
        } else {
            push_nchars(obuf, 0, str_bak, str - str_bak, obuf->prev_ctype);
        }
    }
}

#if 0
int
is_blank_line(char *line, int indent)
{
    int i, is_blank = 0;

    for (i = 0; i < indent; i++) {
	if (line[i] == '\0') {
	    is_blank = 1;
	}
	else if (line[i] != ' ') {
	    break;
	}
    }
    if (i == indent && line[i] == '\0')
	is_blank = 1;
    return is_blank;
}
#endif

static void
fillline(struct readbuffer* obuf, int indent)
{
    push_spaces(obuf, 1, indent - obuf->pos);
    obuf->flag &= ~RB_NFLUSHED;
}

void flushline(struct html_feed_environ* h_env, struct readbuffer* obuf, int indent,
    int force, int width)
{
    TextLineList* buf = h_env->buf;
    FILE* f = h_env->f;
    Str line = obuf->line, pass = NULL;
    char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
         *hidden_under = NULL, *hidden_italic = NULL, *hidden_strike = NULL,
         *hidden_ins = NULL, *hidden_input = NULL, *hidden = NULL;

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && Strlastchar(line) == ' ') {
        Strshrink(line, 1);
        obuf->pos--;
    }

    append_tags(obuf);

    if (obuf->anchor.url)
        hidden = hidden_anchor = has_hidden_link(obuf, HTML_A);
    if (obuf->img_alt) {
        if ((hidden_img = has_hidden_link(obuf, HTML_IMG_ALT)) != NULL) {
            if (!hidden || hidden_img < hidden)
                hidden = hidden_img;
        }
    }
    if (obuf->input_alt.in) {
        if ((hidden_input = has_hidden_link(obuf, HTML_INPUT_ALT)) != NULL) {
            if (!hidden || hidden_input < hidden)
                hidden = hidden_input;
        }
    }
    if (obuf->in_bold) {
        if ((hidden_bold = has_hidden_link(obuf, HTML_B)) != NULL) {
            if (!hidden || hidden_bold < hidden)
                hidden = hidden_bold;
        }
    }
    if (obuf->in_italic) {
        if ((hidden_italic = has_hidden_link(obuf, HTML_I)) != NULL) {
            if (!hidden || hidden_italic < hidden)
                hidden = hidden_italic;
        }
    }
    if (obuf->in_under) {
        if ((hidden_under = has_hidden_link(obuf, HTML_U)) != NULL) {
            if (!hidden || hidden_under < hidden)
                hidden = hidden_under;
        }
    }
    if (obuf->in_strike) {
        if ((hidden_strike = has_hidden_link(obuf, HTML_S)) != NULL) {
            if (!hidden || hidden_strike < hidden)
                hidden = hidden_strike;
        }
    }
    if (obuf->in_ins) {
        if ((hidden_ins = has_hidden_link(obuf, HTML_INS)) != NULL) {
            if (!hidden || hidden_ins < hidden)
                hidden = hidden_ins;
        }
    }
    if (hidden) {
        pass = Strnew_charp(hidden);
        Strshrink(line, line->ptr + line->length - hidden);
    }

    if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && obuf->pos > width) {
        char* tp = &line->ptr[obuf->bp.len - obuf->bp.tlen];
        char* ep = &line->ptr[line->length];

        if (obuf->bp.pos == obuf->pos && tp <= ep && tp > line->ptr && tp[-1] == ' ') {
            bcopy(tp, tp - 1, ep - tp + 1);
            line->length--;
            obuf->pos--;
        }
    }

    if (obuf->anchor.url && !hidden_anchor)
        Strcat_charp(line, "</a>");
    if (obuf->img_alt && !hidden_img)
        Strcat_charp(line, "</img_alt>");
    if (obuf->input_alt.in && !hidden_input)
        Strcat_charp(line, "</input_alt>");
    if (obuf->in_bold && !hidden_bold)
        Strcat_charp(line, "</b>");
    if (obuf->in_italic && !hidden_italic)
        Strcat_charp(line, "</i>");
    if (obuf->in_under && !hidden_under)
        Strcat_charp(line, "</u>");
    if (obuf->in_strike && !hidden_strike)
        Strcat_charp(line, "</s>");
    if (obuf->in_ins && !hidden_ins)
        Strcat_charp(line, "</ins>");

    if (obuf->top_margin > 0) {
        int i;
        struct html_feed_environ h;
        struct readbuffer o;
        struct environment e[1];

        init_henv(&h, &o, e, 1, NULL, width, indent);
        o.line = Strnew_size(width + 20);
        o.pos = obuf->pos;
        o.flag = obuf->flag;
        o.top_margin = -1;
        o.bottom_margin = -1;
        Strcat_charp(o.line, "<pre_int>");
        for (i = 0; i < o.pos; i++)
            Strcat_char(o.line, ' ');
        Strcat_charp(o.line, "</pre_int>");
        for (i = 0; i < obuf->top_margin; i++)
            flushline(h_env, &o, indent, force, width);
    }

    if (force == 1 || obuf->flag & RB_NFLUSHED) {
        TextLine* lbuf = newTextLine(line, obuf->pos);
        if (RB_GET_ALIGN(obuf) == RB_CENTER) {
            align(lbuf, width, ALIGN_CENTER);
        } else if (RB_GET_ALIGN(obuf) == RB_RIGHT) {
            align(lbuf, width, ALIGN_RIGHT);
        } else if (RB_GET_ALIGN(obuf) == RB_LEFT && obuf->flag & RB_INTABLE) {
            align(lbuf, width, ALIGN_LEFT);
        }

        if (lbuf->pos > h_env->maxlimit)
            h_env->maxlimit = lbuf->pos;
        if (buf)
            pushTextLine(buf, lbuf);
        else if (f) {
            Strfputs(Str_conv_to_halfdump(lbuf->line), f);
            fputc('\n', f);
        }
        if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
            h_env->blank_lines = 0;
        else
            h_env->blank_lines++;
    } else {
        char *p = line->ptr, *q;
        Str tmp = Strnew(), tmp2 = Strnew();

#define APPEND(str)                    \
    if (buf)                           \
        appendTextLine(buf, (str), 0); \
    else if (f)                        \
    Strfputs((str), f)

        while (*p) {
            q = p;
            if (sloppy_parse_line(&p)) {
                Strcat_charp_n(tmp, q, p - q);
                if (force == 2) {
                    APPEND(tmp);
                } else
                    Strcat(tmp2, tmp);
                Strclear(tmp);
            }
        }
        if (force == 2) {
            if (pass) {
                APPEND(pass);
            }
            pass = NULL;
        } else {
            if (pass)
                Strcat(tmp2, pass);
            pass = tmp2;
        }
    }

    if (obuf->bottom_margin > 0) {
        int i;
        struct html_feed_environ h;
        struct readbuffer o;
        struct environment e[1];

        init_henv(&h, &o, e, 1, NULL, width, indent);
        o.line = Strnew_size(width + 20);
        o.pos = obuf->pos;
        o.flag = obuf->flag;
        o.top_margin = -1;
        o.bottom_margin = -1;
        Strcat_charp(o.line, "<pre_int>");
        for (i = 0; i < o.pos; i++)
            Strcat_char(o.line, ' ');
        Strcat_charp(o.line, "</pre_int>");
        for (i = 0; i < obuf->bottom_margin; i++)
            flushline(h_env, &o, indent, force, width);
    }
    if (obuf->top_margin < 0 || obuf->bottom_margin < 0)
        return;

    obuf->line = Strnew_size(256);
    obuf->pos = 0;
    obuf->top_margin = 0;
    obuf->bottom_margin = 0;
    Strcopy_charp_n(obuf->prevchar, " ", 1);
    obuf->bp.init_flag = 1;
    obuf->flag &= ~RB_NFLUSHED;
    set_breakpoint(obuf, 0);
    obuf->prev_ctype = PC_ASCII;
    link_stack = NULL;
    fillline(obuf, indent);
    if (pass)
        passthrough(obuf, pass->ptr, 0);
    if (!hidden_anchor && obuf->anchor.url) {
        Str tmp;
        if (obuf->anchor.hseq > 0)
            obuf->anchor.hseq = -obuf->anchor.hseq;
        tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor.hseq);
        Strcat_charp(tmp, html_quote(obuf->anchor.url));
        if (obuf->anchor.target) {
            Strcat_charp(tmp, "\" TARGET=\"");
            Strcat_charp(tmp, html_quote(obuf->anchor.target));
        }
        if (obuf->anchor.referer) {
            Strcat_charp(tmp, "\" REFERER=\"");
            Strcat_charp(tmp, html_quote(obuf->anchor.referer));
        }
        if (obuf->anchor.title) {
            Strcat_charp(tmp, "\" TITLE=\"");
            Strcat_charp(tmp, html_quote(obuf->anchor.title));
        }
        if (obuf->anchor.accesskey) {
            char* c = html_quote_char(obuf->anchor.accesskey);
            Strcat_charp(tmp, "\" ACCESSKEY=\"");
            if (c)
                Strcat_charp(tmp, c);
            else
                Strcat_char(tmp, obuf->anchor.accesskey);
        }
        Strcat_charp(tmp, "\">");
        push_tag(obuf, tmp->ptr, HTML_A);
    }
    if (!hidden_img && obuf->img_alt) {
        Str tmp = Strnew_charp("<IMG_ALT SRC=\"");
        Strcat_charp(tmp, html_quote(obuf->img_alt->ptr));
        Strcat_charp(tmp, "\">");
        push_tag(obuf, tmp->ptr, HTML_IMG_ALT);
    }
    if (!hidden_input && obuf->input_alt.in) {
        Str tmp;
        if (obuf->input_alt.hseq > 0)
            obuf->input_alt.hseq = -obuf->input_alt.hseq;
        tmp = Sprintf("<INPUT_ALT hseq=\"%d\" fid=\"%d\" name=\"%s\" type=\"%s\" value=\"%s\">",
            obuf->input_alt.hseq,
            obuf->input_alt.fid,
            obuf->input_alt.name ? obuf->input_alt.name->ptr : "",
            obuf->input_alt.type ? obuf->input_alt.type->ptr : "",
            obuf->input_alt.value ? obuf->input_alt.value->ptr : "");
        push_tag(obuf, tmp->ptr, HTML_INPUT_ALT);
    }
    if (!hidden_bold && obuf->in_bold)
        push_tag(obuf, "<B>", HTML_B);
    if (!hidden_italic && obuf->in_italic)
        push_tag(obuf, "<I>", HTML_I);
    if (!hidden_under && obuf->in_under)
        push_tag(obuf, "<U>", HTML_U);
    if (!hidden_strike && obuf->in_strike)
        push_tag(obuf, "<S>", HTML_S);
    if (!hidden_ins && obuf->in_ins)
        push_tag(obuf, "<INS>", HTML_INS);
}

void do_blankline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    int indent, int indent_incr, int width)
{
    if (h_env->blank_lines == 0)
        flushline(h_env, obuf, indent, 1, width);
}

void purgeline(struct html_feed_environ* h_env)
{
    char *p, *q;
    Str tmp;
    TextLine* tl;

    if (h_env->buf == NULL || h_env->blank_lines == 0)
        return;

    if (!(tl = rpopTextLine(h_env->buf)))
        return;
    p = tl->line->ptr;
    tmp = Strnew();
    while (*p) {
        q = p;
        if (sloppy_parse_line(&p)) {
            Strcat_charp_n(tmp, q, p - q);
        }
    }
    appendTextLine(h_env->buf, tmp, 0);
    h_env->blank_lines--;
}

static int
close_effect0(struct readbuffer* obuf, int cmd)
{
    int i;
    char* p;

    for (i = obuf->tag_sp - 1; i >= 0; i--) {
        if (obuf->tag_stack[i]->cmd == cmd)
            break;
    }
    if (i >= 0) {
        obuf->tag_sp--;
        bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
            (obuf->tag_sp - i) * sizeof(struct cmdtable*));
        return 1;
    } else if ((p = has_hidden_link(obuf, cmd)) != NULL) {
        passthrough(obuf, p, 1);
        return 1;
    }
    return 0;
}

static void
close_anchor(struct HtmlBuilder* hb,
    struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    if (obuf->anchor.url) {
        int i;
        char* p = NULL;
        int is_erased = 0;

        for (i = obuf->tag_sp - 1; i >= 0; i--) {
            if (obuf->tag_stack[i]->cmd == HTML_A)
                break;
        }
        if (i < 0 && obuf->anchor.hseq > 0 && Strlastchar(obuf->line) == ' ') {
            Strshrink(obuf->line, 1);
            obuf->pos--;
            is_erased = 1;
        }

        if (i >= 0 || (p = has_hidden_link(obuf, HTML_A))) {
            if (obuf->anchor.hseq > 0) {
                HTMLlineproc0(hb, ANSP, h_env, true);
                Strcopy_charp_n(obuf->prevchar, " ", 1);
            } else {
                if (i >= 0) {
                    obuf->tag_sp--;
                    bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
                        (obuf->tag_sp - i) * sizeof(struct cmdtable*));
                } else {
                    passthrough(obuf, p, 1);
                }
                bzero((void*)&obuf->anchor, sizeof(obuf->anchor));
                return;
            }
            is_erased = 0;
        }
        if (is_erased) {
            Strcat_char(obuf->line, ' ');
            obuf->pos++;
        }

        push_tag(obuf, "</a>", HTML_N_A);
    }
    bzero((void*)&obuf->anchor, sizeof(obuf->anchor));
}

void save_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
        bcopy(obuf->fontstat, obuf->fontstat_stack[obuf->fontstat_sp],
            FONTSTAT_SIZE);
    if (obuf->fontstat_sp < INT_MAX)
        obuf->fontstat_sp++;
    if (obuf->in_bold)
        push_tag(obuf, "</b>", HTML_N_B);
    if (obuf->in_italic)
        push_tag(obuf, "</i>", HTML_N_I);
    if (obuf->in_under)
        push_tag(obuf, "</u>", HTML_N_U);
    if (obuf->in_strike)
        push_tag(obuf, "</s>", HTML_N_S);
    if (obuf->in_ins)
        push_tag(obuf, "</ins>", HTML_N_INS);
    bzero(obuf->fontstat, FONTSTAT_SIZE);
}

void restore_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    if (obuf->fontstat_sp > 0)
        obuf->fontstat_sp--;
    if (obuf->fontstat_sp < FONT_STACK_SIZE)
        bcopy(obuf->fontstat_stack[obuf->fontstat_sp], obuf->fontstat,
            FONTSTAT_SIZE);
    if (obuf->in_bold)
        push_tag(obuf, "<b>", HTML_B);
    if (obuf->in_italic)
        push_tag(obuf, "<i>", HTML_I);
    if (obuf->in_under)
        push_tag(obuf, "<u>", HTML_U);
    if (obuf->in_strike)
        push_tag(obuf, "<s>", HTML_S);
    if (obuf->in_ins)
        push_tag(obuf, "<ins>", HTML_INS);
}

Str process_img(struct HtmlBuilder* hb, struct parsed_tag* tag, int width)
{
    const char *r, *r2 = NULL, *s;

    int w, i, nw, ni = 1, n, w0 = -1, i0 = -1;
    int align, xoffset, yoffset, top, bottom, ismap = 0;
    int use_image = getRuntime()->activeImage && getRuntime()->displayImage;
    int pre_int = FALSE, ext_pre_int = FALSE;
    Str tmp = Strnew();

    const char* p;
    if (!parsedtag_get_value(tag, ATTR_SRC, &p))
        return tmp;
    p = url_encode(remove_space(p), hb->cur_baseURL, hb->cur_document_charset);
    const char* q = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &q);
    if (!getRuntime()->pseudoInlines && (q == NULL || (*q == '\0' && getRuntime()->ignore_null_img_alt)))
        return tmp;
    const char* t = q;
    parsedtag_get_value(tag, ATTR_TITLE, &t);
    w = -1;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
        if (w < 0) {
            if (width > 0)
                w = (int)(-width * getRuntime()->pixel_per_char * w / 100 + 0.5);
            else
                w = -1;
        }
        if (use_image) {
            if (w > 0) {
                w = (int)(w * getRuntime()->image_scale / 100 + 0.5);
                if (w == 0)
                    w = 1;
                else if (w > MAX_IMAGE_SIZE)
                    w = MAX_IMAGE_SIZE;
            }
        }
    }
    i = -1;
    if (use_image) {
        if (parsedtag_get_value(tag, ATTR_HEIGHT, &i)) {
            if (i > 0) {
                i = (int)(i * getRuntime()->image_scale / 100 + 0.5);
                if (i == 0)
                    i = 1;
                else if (i > MAX_IMAGE_SIZE)
                    i = MAX_IMAGE_SIZE;
            } else {
                i = -1;
            }
        }
        align = -1;
        parsedtag_get_value(tag, ATTR_ALIGN, &align);
        ismap = 0;
        if (parsedtag_exists(tag, ATTR_ISMAP))
            ismap = 1;
    } else
        parsedtag_get_value(tag, ATTR_HEIGHT, &i);
    r = NULL;
    parsedtag_get_value(tag, ATTR_USEMAP, &r);
    if (parsedtag_exists(tag, ATTR_PRE_INT))
        ext_pre_int = TRUE;

    tmp = Strnew_size(128);
    if (use_image) {
        switch (align) {
        case ALIGN_LEFT:
            Strcat_charp(tmp, "<div_int align=left>");
            break;
        case ALIGN_CENTER:
            Strcat_charp(tmp, "<div_int align=center>");
            break;
        case ALIGN_RIGHT:
            Strcat_charp(tmp, "<div_int align=right>");
            break;
        }
    }
    if (r) {
        Str tmp2;
        r2 = strchr(r, '#');
        s = "<form_int method=internal action=map>";
        tmp2 = process_form(hb, parse_tag(&s, TRUE));
        if (tmp2)
            Strcat(tmp, tmp2);
        Strcat(tmp, Sprintf("<input_alt fid=\"%d\" "
                            "type=hidden name=link value=\"",
                        cur_form_id(hb)));
        Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
        Strcat(tmp, Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                            "type=submit no_effect=true>",
                        hb->cur_hseq++, cur_form_id(hb)));
    }
    if (use_image) {
        w0 = w;
        i0 = i;
        if (w < 0 || i < 0) {
            struct Image image;
            struct Url u;

            parseURL2(p, &u, hb->cur_baseURL);
            image.url = parsedURL2Str(&u)->ptr;
            if (!uncompressed_file_type(u.file, &image.ext))
                image.ext = filename_extension(u.file, TRUE);
            image.cache = NULL;
            image.width = w;
            image.height = i;

            image.cache = getImage(&image, hb->cur_baseURL, IMG_FLAG_SKIP);
            if (image.cache && image.cache->width > 0 && image.cache->height > 0) {
                w = w0 = image.cache->width;
                i = i0 = image.cache->height;
            }
            if (w < 0)
                w = 8 * getRuntime()->pixel_per_char;
            if (i < 0)
                i = getRuntime()->pixel_per_line;
        }
        if (getRuntime()->enable_inline_image) {
            nw = (w > 1) ? ((w - 1) / getRuntime()->pixel_per_char_i + 1) : 1;
            ni = (i > 1) ? ((i - 1) / getRuntime()->pixel_per_line_i + 1) : 1;
        } else {
            nw = (w > 3) ? (int)((w - 3) / getRuntime()->pixel_per_char + 1) : 1;
            ni = (i > 3) ? (int)((i - 3) / getRuntime()->pixel_per_line + 1) : 1;
        }
        Strcat(tmp,
            Sprintf("<pre_int><img_alt hseq=\"%d\" src=\"", hb->cur_iseq++));
        pre_int = TRUE;
    } else {
        if (w < 0)
            w = 12 * getRuntime()->pixel_per_char;
        nw = w ? (int)((w - 1) / getRuntime()->pixel_per_char + 1) : 1;
        if (r) {
            Strcat_charp(tmp, "<pre_int>");
            pre_int = TRUE;
        }
        Strcat_charp(tmp, "<img_alt src=\"");
    }
    Strcat_charp(tmp, html_quote(p));
    Strcat_charp(tmp, "\"");
    if (t) {
        Strcat_charp(tmp, " title=\"");
        Strcat_charp(tmp, html_quote(t));
        Strcat_charp(tmp, "\"");
    }
    if (use_image) {
        if (w0 >= 0)
            Strcat(tmp, Sprintf(" width=%d", w0));
        if (i0 >= 0)
            Strcat(tmp, Sprintf(" height=%d", i0));
        switch (align) {
        case ALIGN_MIDDLE:
            if (!getRuntime()->enable_inline_image) {
                top = ni / 2;
                bottom = top;
                if (top * 2 == ni)
                    yoffset = (int)(((ni + 1) * getRuntime()->pixel_per_line - i) / 2);
                else
                    yoffset = (int)((ni * getRuntime()->pixel_per_line - i) / 2);
                break;
            }
        case ALIGN_TOP:
            top = 0;
            bottom = ni - 1;
            yoffset = 0;
            break;
        case ALIGN_BOTTOM:
            top = ni - 1;
            bottom = 0;
            yoffset = (int)(ni * getRuntime()->pixel_per_line - i);
            break;
        default:
            top = ni - 1;
            bottom = 0;
            if (ni == 1 && ni * getRuntime()->pixel_per_line > i)
                yoffset = 0;
            else {
                yoffset = (int)(ni * getRuntime()->pixel_per_line - i);
                if (yoffset <= -2)
                    yoffset++;
            }
            break;
        }

        if (getRuntime()->enable_inline_image)
            xoffset = 0;
        else
            xoffset = (int)((nw * getRuntime()->pixel_per_char - w) / 2);

        if (xoffset)
            Strcat(tmp, Sprintf(" xoffset=%d", xoffset));
        if (yoffset)
            Strcat(tmp, Sprintf(" yoffset=%d", yoffset));
        if (top)
            Strcat(tmp, Sprintf(" top_margin=%d", top));
        if (bottom)
            Strcat(tmp, Sprintf(" bottom_margin=%d", bottom));
        if (r) {
            Strcat_charp(tmp, " usemap=\"");
            Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
            Strcat_charp(tmp, "\"");
        }
        if (ismap)
            Strcat_charp(tmp, " ismap");
    }

    Strcat_charp(tmp, ">");
    if (q != NULL && *q == '\0' && getRuntime()->ignore_null_img_alt)
        q = NULL;
    if (q != NULL) {
        n = get_strwidth(q);
        if (use_image) {
            if (n > nw) {
                const char* r;
                for (r = q, n = 0; *r; r += get_mclen(r), n += get_mcwidth(r)) {
                    if (n + get_mcwidth(r) > nw)
                        break;
                }
                Strcat_charp(tmp, html_quote(Strnew_charp_n(q, r - q)->ptr));
            } else
                Strcat_charp(tmp, html_quote(q));
        } else
            Strcat_charp(tmp, html_quote(q));
        goto img_end;
    }
    if (w > 0 && i > 0) {
        /* guess what the image is! */
        if (w < 32 && i < 48) {
            /* must be an icon or space */
            n = 1;
            if (strcasestr(p, "space") || strcasestr(p, "blank"))
                Strcat_charp(tmp, "_");
            else {
                if (w * i < 8 * 16)
                    Strcat_charp(tmp, "*");
                else {
                    if (!pre_int) {
                        Strcat_charp(tmp, "<pre_int>");
                        pre_int = TRUE;
                    }
                    push_symbol(tmp, IMG_SYMBOL, symbol_width, 1);
                    n = symbol_width;
                }
            }
            goto img_end;
        }
        if (w > 200 && i < 13) {
            /* must be a horizontal line */
            if (!pre_int) {
                Strcat_charp(tmp, "<pre_int>");
                pre_int = TRUE;
            }
            w = w / getRuntime()->pixel_per_char / symbol_width;
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, symbol_width, w);
            n = w * symbol_width;
            goto img_end;
        }
    }

    q = p;
    for (; *q; q++)
        ;
    while (q > p && *q != '/')
        q--;
    if (*q == '/')
        q++;
    Strcat_char(tmp, '[');
    n = 1;
    p = q;
    for (; *q; q++) {
        if (!IS_ALNUM(*q) && *q != '_' && *q != '-') {
            break;
        }
        Strcat_char(tmp, *q);
        n++;
        if (n + 1 >= nw)
            break;
    }
    Strcat_char(tmp, ']');
    n++;
img_end:
#ifdef USE_IMAGE
    if (use_image) {
        for (; n < nw; n++)
            Strcat_char(tmp, ' ');
    }
#endif
    Strcat_charp(tmp, "</img_alt>");
    if (pre_int && !ext_pre_int)
        Strcat_charp(tmp, "</pre_int>");
    if (r) {
        Strcat_charp(tmp, "</input_alt>");
        process_n_form(hb);
    }
#ifdef USE_IMAGE
    if (use_image) {
        switch (align) {
        case ALIGN_RIGHT:
        case ALIGN_CENTER:
        case ALIGN_LEFT:
            Strcat_charp(tmp, "</div_int>");
            break;
        }
    }
#endif
    return tmp;
}

Str process_anchor(struct HtmlBuilder* hb,
    struct parsed_tag* tag, const char* tagbuf)
{
    if (parsedtag_need_reconstruct(tag)) {
        parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", hb->cur_hseq++)->ptr);
        return parsedtag2str(tag);
    } else {
        Str tmp = Sprintf("<a hseq=\"%d\"", hb->cur_hseq++);
        Strcat_charp(tmp, tagbuf + 2);
        return tmp;
    }
}

Str process_input(struct HtmlBuilder* hb, struct parsed_tag* tag)
{
    int i = 20, v, x, y, z, iw, ih, size = 20;
    const char *q, *p, *r, *p2, *s;
    Str tmp = NULL;
    char* qq = "";
    int qlen = 0;

    if (cur_form_id(hb) < 0) {
        const char* s = "<form_int method=internal action=none>";
        tmp = process_form(hb, parse_tag(&s, TRUE));
    }
    if (tmp == NULL)
        tmp = Strnew();

    p = "text";
    parsedtag_get_value(tag, ATTR_TYPE, &p);
    q = NULL;
    parsedtag_get_value(tag, ATTR_VALUE, &q);
    r = "";
    parsedtag_get_value(tag, ATTR_NAME, &r);
    parsedtag_get_value(tag, ATTR_SIZE, &size);
    if (size > MAX_INPUT_SIZE)
        size = MAX_INPUT_SIZE;
    parsedtag_get_value(tag, ATTR_MAXLENGTH, &i);
    p2 = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &p2);
    x = parsedtag_exists(tag, ATTR_CHECKED);
    y = parsedtag_exists(tag, ATTR_ACCEPT);
    z = parsedtag_exists(tag, ATTR_READONLY);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
        return NULL;

    if (!q) {
        switch (v) {
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            q = "SUBMIT";
            break;
        case FORM_INPUT_RESET:
            q = "RESET";
            break;
            /* if no VALUE attribute is specified in
             * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used
             * as a default value. It is not a part of HTML4.0
             * specification, but an imitation of Netscape behaviour.
             */
        case FORM_INPUT_CHECKBOX:
            q = "on";
        }
    }
    /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
    if (v == FORM_INPUT_FILE)
        q = NULL;
    if (q) {
        qq = html_quote(q);
        qlen = get_strwidth(q);
    }

    Strcat_charp(tmp, "<pre_int>");
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
        if (getRuntime()->displayLinkNumber)
            Strcat(tmp, getLinkNumberStr(hb, 0));
        Strcat_char(tmp, '[');
        break;
    case FORM_INPUT_RADIO:
        if (getRuntime()->displayLinkNumber)
            Strcat(tmp, getLinkNumberStr(hb, 0));
        Strcat_char(tmp, '(');
    }
    Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                        "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                    hb->cur_hseq++, cur_form_id(hb), html_quote(p), html_quote(r), size, i, qq));
    if (x)
        Strcat_charp(tmp, " checked");
    if (y)
        Strcat_charp(tmp, " accept");
    if (z)
        Strcat_charp(tmp, " readonly");
    Strcat_char(tmp, '>');

    if (v == FORM_INPUT_HIDDEN)
        Strcat_charp(tmp, "</input_alt></pre_int>");
    else {
        switch (v) {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            Strcat_charp(tmp, "<u>");
            break;
        case FORM_INPUT_IMAGE:
            s = NULL;
            parsedtag_get_value(tag, ATTR_SRC, &s);
            if (s) {
                Strcat(tmp, Sprintf("<img src=\"%s\"", html_quote(s)));
                if (p2)
                    Strcat(tmp, Sprintf(" alt=\"%s\"", html_quote(p2)));
                if (parsedtag_get_value(tag, ATTR_WIDTH, &iw))
                    Strcat(tmp, Sprintf(" width=\"%d\"", iw));
                if (parsedtag_get_value(tag, ATTR_HEIGHT, &ih))
                    Strcat(tmp, Sprintf(" height=\"%d\"", ih));
                Strcat_charp(tmp, " pre_int>");
                Strcat_charp(tmp, "</input_alt></pre_int>");
                return tmp;
            }
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            if (getRuntime()->displayLinkNumber)
                Strcat(tmp, getLinkNumberStr(hb, -1));
            Strcat_charp(tmp, "[");
            break;
        }
        switch (v) {
        case FORM_INPUT_PASSWORD:
            i = 0;
            if (q) {
                for (; i < qlen && i < size; i++)
                    Strcat_char(tmp, '*');
            }
            for (; i < size; i++)
                Strcat_char(tmp, ' ');
            break;
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            if (q)
                Strcat(tmp, textfieldrep(Strnew_charp(q), size));
            else {
                for (i = 0; i < size; i++)
                    Strcat_char(tmp, ' ');
            }
            break;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            if (p2)
                Strcat_charp(tmp, html_quote(p2));
            else
                Strcat_charp(tmp, qq);
            break;
        case FORM_INPUT_RESET:
            Strcat_charp(tmp, qq);
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (x)
                Strcat_char(tmp, '*');
            else
                Strcat_char(tmp, ' ');
            break;
        }
        switch (v) {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            Strcat_charp(tmp, "</u>");
            break;
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            Strcat_charp(tmp, "]");
        }
        Strcat_charp(tmp, "</input_alt>");
        switch (v) {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
        case FORM_INPUT_CHECKBOX:
            Strcat_char(tmp, ']');
            break;
        case FORM_INPUT_RADIO:
            Strcat_char(tmp, ')');
        }
        Strcat_charp(tmp, "</pre_int>");
    }
    return tmp;
}

Str process_button(struct HtmlBuilder* hb, struct parsed_tag* tag)
{
    Str tmp = NULL;
    char *p, *q, *r, *qq = "";
    int v;

    if (cur_form_id(hb) < 0) {
        const char* s = "<form_int method=internal action=none>";
        tmp = process_form(hb, parse_tag(&s, TRUE));
    }
    if (tmp == NULL)
        tmp = Strnew();

    p = "submit";
    parsedtag_get_value(tag, ATTR_TYPE, &p);
    q = NULL;
    parsedtag_get_value(tag, ATTR_VALUE, &q);
    r = "";
    parsedtag_get_value(tag, ATTR_NAME, &r);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
        return NULL;

    switch (v) {
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
        break;
    default:
        p = "submit";
        v = FORM_INPUT_SUBMIT;
        break;
    }

    if (!q) {
        switch (v) {
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            q = "SUBMIT";
            break;
        case FORM_INPUT_RESET:
            q = "RESET";
            break;
        }
    }
    if (q) {
        qq = html_quote(q);
    }

    /*    Strcat_charp(tmp, "<pre_int>"); */
    Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                        "name=\"%s\" value=\"%s\">",
                    hb->cur_hseq++, cur_form_id(hb), html_quote(p), html_quote(r), qq));
    return tmp;
}

Str process_n_button(void)
{
    Str tmp = Strnew();
    Strcat_charp(tmp, "</input_alt>");
    /*    Strcat_charp(tmp, "</pre_int>"); */
    return tmp;
}

Str process_select(struct HtmlBuilder* hb, struct parsed_tag* tag)
{
    Str tmp = NULL;
    char* p;

    if (cur_form_id(hb) < 0) {
        const char* s = "<form_int method=internal action=none>";
        tmp = process_form(hb, parse_tag(&s, TRUE));
    }

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    hb->cur_select = Strnew_charp(p);
    hb->select_is_multiple = parsedtag_exists(tag, ATTR_MULTIPLE);

    if (!hb->select_is_multiple) {
        hb->select_str = Strnew_charp("<pre_int>");
        if (getRuntime()->displayLinkNumber)
            Strcat(hb->select_str, getLinkNumberStr(hb, 0));
        Strcat(hb->select_str, Sprintf("[<input_alt hseq=\"%d\" "
                                       "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                                   hb->cur_hseq++, cur_form_id(hb), html_quote(p), hb->n_select));
        Strcat_charp(hb->select_str, ">");
        if (hb->n_select == hb->max_select) {
            hb->max_select *= 2;
            hb->select_option = New_Reuse(struct FormSelectOption, hb->select_option, hb->max_select);
        }
        hb->select_option[hb->n_select].first = NULL;
        hb->select_option[hb->n_select].last = NULL;
        hb->cur_option_maxwidth = 0;
    } else
        hb->select_str = Strnew();
    hb->cur_option = NULL;
    hb->cur_status = R_ST_NORMAL;
    hb->n_selectitem = 0;
    return tmp;
}

Str process_n_select(struct HtmlBuilder* hb)
{
    if (hb->cur_select == NULL)
        return NULL;
    process_option(hb);
    if (!hb->select_is_multiple) {
        if (hb->select_option[hb->n_select].first) {
            struct FormItemList sitem;
            chooseSelectOption(&sitem, hb->select_option[hb->n_select].first);
            Strcat(hb->select_str, textfieldrep(sitem.label, hb->cur_option_maxwidth));
        }
        Strcat_charp(hb->select_str, "</input_alt>]</pre_int>");
        hb->n_select++;
    } else

        Strcat_charp(hb->select_str, "<br>");
    hb->cur_select = NULL;
    hb->n_selectitem = 0;
    return hb->select_str;
}

void feed_select(struct HtmlBuilder* hb, const char* str)
{
    Str tmp = Strnew();
    int prev_status = hb->cur_status;
    static int prev_spaces = -1;
    const char* p;

    if (hb->cur_select == NULL)
        return;
    while (read_token(tmp, &str, &hb->cur_status, 0, 0)) {
        if (hb->cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
            continue;
        p = tmp->ptr;
        if (tmp->ptr[0] == '<' && Strlastchar(tmp) == '>') {
            struct parsed_tag* tag;
            const char* q;
            if (!(tag = parse_tag(&p, FALSE)))
                continue;
            switch (tag->tagid) {
            case HTML_OPTION:
                process_option(hb);
                hb->cur_option = Strnew();
                if (parsedtag_get_value(tag, ATTR_VALUE, &q))
                    hb->cur_option_value = Strnew_charp(q);
                else
                    hb->cur_option_value = NULL;
                if (parsedtag_get_value(tag, ATTR_LABEL, &q))
                    hb->cur_option_label = Strnew_charp(q);
                else
                    hb->cur_option_label = NULL;
                hb->cur_option_selected = parsedtag_exists(tag, ATTR_SELECTED);
                prev_spaces = -1;
                break;
            case HTML_N_OPTION:
                /* do nothing */
                break;
            default:
                /* never happen */
                break;
            }
        } else if (hb->cur_option) {
            while (*p) {
                if (IS_SPACE(*p) && prev_spaces != 0) {
                    p++;
                    if (prev_spaces > 0)
                        prev_spaces++;
                } else {
                    if (IS_SPACE(*p))
                        prev_spaces = 1;
                    else
                        prev_spaces = 0;
                    if (*p == '&')
                        Strcat_charp(hb->cur_option, getescapecmd(&p));
                    else
                        Strcat_char(hb->cur_option, *(p++));
                }
            }
        }
    }
}

void process_option(struct HtmlBuilder* hb)
{
    char begin_char = '[', end_char = ']';

    if (hb->cur_select == NULL || hb->cur_option == NULL)
        return;
    while (hb->cur_option->length > 0 && IS_SPACE(Strlastchar(hb->cur_option)))
        Strshrink(hb->cur_option, 1);
    if (hb->cur_option_value == NULL)
        hb->cur_option_value = hb->cur_option;
    if (hb->cur_option_label == NULL)
        hb->cur_option_label = hb->cur_option;
    int len;
    if (!hb->select_is_multiple) {
        len = get_Str_strwidth(hb->cur_option_label);
        if (len > hb->cur_option_maxwidth)
            hb->cur_option_maxwidth = len;
        addSelectOption(&hb->select_option[hb->n_select],
            hb->cur_option_value,
            hb->cur_option_label, hb->cur_option_selected);
        return;
    }

    if (!hb->select_is_multiple) {
        begin_char = '(';
        end_char = ')';
    }
    Strcat(hb->select_str, Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                                   "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                               begin_char, hb->cur_hseq++, cur_form_id(hb), hb->select_is_multiple ? "checkbox" : "radio", html_quote(hb->cur_select->ptr), html_quote(hb->cur_option_value->ptr)));
    if (hb->cur_option_selected)
        Strcat_charp(hb->select_str, " checked>*</input_alt>");
    else
        Strcat_charp(hb->select_str, "> </input_alt>");
    Strcat_char(hb->select_str, end_char);
    Strcat_charp(hb->select_str, html_quote(hb->cur_option_label->ptr));
    Strcat_charp(hb->select_str, "</pre_int>");
    hb->n_selectitem++;
}

Str process_textarea(struct HtmlBuilder* hb, struct parsed_tag* tag, int width)
{
    Str tmp = NULL;
    char* p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

    if (cur_form_id(hb) < 0) {
        const char* s = "<form_int method=internal action=none>";
        tmp = process_form(hb, parse_tag(&s, TRUE));
    }

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    hb->cur_textarea = Strnew_charp(p);
    hb->cur_textarea_size = 20;
    if (parsedtag_get_value(tag, ATTR_COLS, &p)) {
        hb->cur_textarea_size = atoi(p);
        if (strlen(p) > 0 && p[strlen(p) - 1] == '%')
            hb->cur_textarea_size = width * hb->cur_textarea_size / 100 - 2;
        if (hb->cur_textarea_size <= 0) {
            hb->cur_textarea_size = 20;
        } else if (hb->cur_textarea_size > TEXTAREA_ATTR_COL_MAX) {
            hb->cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
        }
    }
    hb->cur_textarea_rows = 1;
    if (parsedtag_get_value(tag, ATTR_ROWS, &p)) {
        hb->cur_textarea_rows = atoi(p);
        if (hb->cur_textarea_rows <= 0) {
            hb->cur_textarea_rows = 1;
        } else if (hb->cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX) {
            hb->cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
        }
    }
    hb->cur_textarea_readonly = parsedtag_exists(tag, ATTR_READONLY);
    if (hb->n_textarea >= hb->max_textarea) {
        hb->max_textarea *= 2;
        hb->textarea_str = New_Reuse(Str, hb->textarea_str, hb->max_textarea);
    }
    hb->textarea_str[hb->n_textarea] = Strnew();
    hb->ignore_nl_textarea = TRUE;

    return tmp;
}

Str process_n_textarea(struct HtmlBuilder* hb)
{
    Str tmp;
    int i;

    if (hb->cur_textarea == NULL)
        return NULL;

    tmp = Strnew();
    Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                        "type=textarea name=\"%s\" size=%d rows=%d "
                        "top_margin=%d textareanumber=%d",
                    hb->cur_hseq, cur_form_id(hb), html_quote(hb->cur_textarea->ptr), hb->cur_textarea_size, hb->cur_textarea_rows, hb->cur_textarea_rows - 1, hb->n_textarea));
    if (hb->cur_textarea_readonly)
        Strcat_charp(tmp, " readonly");
    Strcat_charp(tmp, "><u>");
    for (i = 0; i < hb->cur_textarea_size; i++)
        Strcat_char(tmp, ' ');
    Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
    hb->cur_hseq++;
    hb->n_textarea++;
    hb->cur_textarea = NULL;

    return tmp;
}

void feed_textarea(struct HtmlBuilder* hb, const char* str)
{
    if (hb->cur_textarea == NULL)
        return;
    if (hb->ignore_nl_textarea) {
        if (*str == '\r')
            str++;
        if (*str == '\n')
            str++;
    }
    hb->ignore_nl_textarea = FALSE;
    while (*str) {
        if (*str == '&')
            Strcat_charp(hb->textarea_str[hb->n_textarea], getescapecmd(&str));
        else if (*str == '\n') {
            Strcat_charp(hb->textarea_str[hb->n_textarea], "\r\n");
            str++;
        } else if (*str == '\r')
            str++;
        else
            Strcat_char(hb->textarea_str[hb->n_textarea], *(str++));
    }
}

static Str
process_hr(struct parsed_tag* tag, int width, int indent_width)
{
    Str tmp = Strnew_charp("<nobr>");
    int w = 0;
    int x = ALIGN_CENTER;
#define HR_ATTR_WIDTH_MAX 65535

    if (width > indent_width)
        width -= indent_width;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
        if (w > HR_ATTR_WIDTH_MAX) {
            w = HR_ATTR_WIDTH_MAX;
        }
        w = REAL_WIDTH(w, width);
    } else {
        w = width;
    }

    parsedtag_get_value(tag, ATTR_ALIGN, &x);
    switch (x) {
    case ALIGN_CENTER:
        Strcat_charp(tmp, "<div_int align=center>");
        break;
    case ALIGN_RIGHT:
        Strcat_charp(tmp, "<div_int align=right>");
        break;
    case ALIGN_LEFT:
        Strcat_charp(tmp, "<div_int align=left>");
        break;
    }
    w /= symbol_width;
    if (w <= 0)
        w = 1;
    push_symbol(tmp, HR_SYMBOL, symbol_width, w);
    Strcat_charp(tmp, "</div_int></nobr>");
    return tmp;
}

#ifdef USE_M17N
static char*
check_charset(char* p)
{
    return wc_guess_charset(p, 0) ? p : NULL;
}

static char*
check_accept_charset(char* ac)
{
    char *s = ac, *e;

    while (*s) {
        while (*s && (IS_SPACE(*s) || *s == ','))
            s++;
        if (!*s)
            break;
        e = s;
        while (*e && !(IS_SPACE(*e) || *e == ','))
            e++;
        if (wc_guess_charset(Strnew_charp_n(s, e - s)->ptr, 0))
            return ac;
        s = e;
    }
    return NULL;
}
#endif

static Str
process_form_int(struct HtmlBuilder* hb,
    struct parsed_tag* tag, int fid)
{
    char *p, *q, *r, *s, *tg, *n;

    p = "get";
    parsedtag_get_value(tag, ATTR_METHOD, &p);
    q = "!CURRENT_URL!";
    parsedtag_get_value(tag, ATTR_ACTION, &q);
    q = url_encode(remove_space(q), hb->cur_baseURL, hb->cur_document_charset);
    r = NULL;
    if (parsedtag_get_value(tag, ATTR_ACCEPT_CHARSET, &r))
        r = check_accept_charset(r);
    if (!r && parsedtag_get_value(tag, ATTR_CHARSET, &r))
        r = check_charset(r);
    s = NULL;
    parsedtag_get_value(tag, ATTR_ENCTYPE, &s);
    tg = NULL;
    parsedtag_get_value(tag, ATTR_TARGET, &tg);
    n = NULL;
    parsedtag_get_value(tag, ATTR_NAME, &n);

    if (fid < 0) {
        hb->form_max++;
        hb->form_sp++;
        fid = hb->form_max;
    } else { /* <form_int> */
        if (hb->form_max < fid)
            hb->form_max = fid;
        hb->form_sp = fid;
    }
    if (hb->forms_size == 0) {
        hb->forms_size = INITIAL_FORM_SIZE;
        hb->forms = New_N(struct FormList*, hb->forms_size);
        hb->form_stack = NewAtom_N(int, hb->forms_size);
    }
    if (hb->forms_size <= hb->form_max) {
        hb->forms_size += hb->form_max;
        hb->forms = New_Reuse(struct FormList*, hb->forms, hb->forms_size);
        hb->form_stack = New_Reuse(int, hb->form_stack, hb->forms_size);
    }
    hb->form_stack[hb->form_sp] = fid;

    if (w3m_halfdump) {
        Str tmp = Sprintf("<form_int fid=\"%d\" action=\"%s\" method=\"%s\"",
            fid, html_quote(q), html_quote(p));
        if (s)
            Strcat(tmp, Sprintf(" enctype=\"%s\"", html_quote(s)));
        if (tg)
            Strcat(tmp, Sprintf(" target=\"%s\"", html_quote(tg)));
        if (n)
            Strcat(tmp, Sprintf(" name=\"%s\"", html_quote(n)));
#ifdef USE_M17N
        if (r)
            Strcat(tmp, Sprintf(" accept-charset=\"%s\"", html_quote(r)));
#endif
        Strcat_charp(tmp, ">");
        return tmp;
    }

    hb->forms[fid] = newFormList(q, p, r, s, tg, n, NULL);
    return NULL;
}

Str process_form(struct HtmlBuilder* hb, struct parsed_tag* tag)
{
    return process_form_int(hb, tag, -1);
}

Str process_n_form(struct HtmlBuilder* hb)
{
    if (hb->form_sp >= 0)
        hb->form_sp--;
    return NULL;
}

static void
clear_ignore_p_flag(int cmd, struct readbuffer* obuf)
{
    static int clear_flag_cmd[] = {
        HTML_HR, HTML_UNKNOWN
    };
    int i;

    for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++) {
        if (cmd == clear_flag_cmd[i]) {
            obuf->flag &= ~RB_IGNORE_P;
            return;
        }
    }
}

static void
set_alignment(struct readbuffer* obuf, struct parsed_tag* tag)
{
    long flag = -1;
    int align;

    if (parsedtag_get_value(tag, ATTR_ALIGN, &align)) {
        switch (align) {
        case ALIGN_CENTER:
            if (getRuntime()->DisableCenter)
                flag = RB_LEFT;
            else
                flag = RB_CENTER;
            break;
        case ALIGN_RIGHT:
            flag = RB_RIGHT;
            break;
        case ALIGN_LEFT:
            flag = RB_LEFT;
        }
    }
    RB_SAVE_FLAG(obuf);
    if (flag != -1) {
        RB_SET_ALIGN(obuf, flag);
    }
}

#ifdef ID_EXT
static void
process_idattr(struct readbuffer* obuf, int cmd, struct parsed_tag* tag)
{
    char *id = NULL, *framename = NULL;
    Str idtag = NULL;

    /*
     * HTML_TABLE is handled by the other process.
     */
    if (cmd == HTML_TABLE)
        return;

    parsedtag_get_value(tag, ATTR_ID, &id);
    parsedtag_get_value(tag, ATTR_FRAMENAME, &framename);
    if (id == NULL)
        return;
    if (framename)
        idtag = Sprintf("<_id id=\"%s\" framename=\"%s\">",
            html_quote(id), html_quote(framename));
    else
        idtag = Sprintf("<_id id=\"%s\">", html_quote(id));
    push_tag(obuf, idtag->ptr, HTML_NOP);
}
#endif /* ID_EXT */

#define CLOSE_P                                                            \
    if (obuf->flag & RB_P) {                                               \
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit); \
        RB_RESTORE_FLAG(obuf);                                             \
        obuf->flag &= ~RB_P;                                               \
    }

#define HTML5_CLOSE_A                      \
    do {                                   \
        if (obuf->flag & RB_HTML5) {       \
            close_anchor(hb, h_env, obuf); \
        }                                  \
    } while (0)

#define CLOSE_A                            \
    do {                                   \
        CLOSE_P;                           \
        if (!(obuf->flag & RB_HTML5)) {    \
            close_anchor(hb, h_env, obuf); \
        }                                  \
    } while (0)

#define CLOSE_DT                                \
    if (obuf->flag & RB_IN_DT) {                \
        obuf->flag &= ~RB_IN_DT;                \
        HTMLlineproc0(hb, "</b>", h_env, true); \
    }

#define PUSH_ENV(cmd)                                                              \
    if (++h_env->envc_real < h_env->nenv) {                                        \
        ++h_env->envc;                                                             \
        envs[h_env->envc].env = cmd;                                               \
        envs[h_env->envc].count = 0;                                               \
        if (h_env->envc <= MAX_INDENT_LEVEL)                                       \
            envs[h_env->envc].indent = envs[h_env->envc - 1].indent + getRuntime()->IndentIncr; \
        else                                                                       \
            envs[h_env->envc].indent = envs[h_env->envc - 1].indent;               \
    }

#define PUSH_ENV_NOINDENT(cmd)                                   \
    if (++h_env->envc_real < h_env->nenv) {                      \
        ++h_env->envc;                                           \
        envs[h_env->envc].env = cmd;                             \
        envs[h_env->envc].count = 0;                             \
        envs[h_env->envc].indent = envs[h_env->envc - 1].indent; \
    }

#define POP_ENV                           \
    if (h_env->envc_real-- < h_env->nenv) \
        h_env->envc--;

static int
ul_type(struct parsed_tag* tag, int default_type)
{
    char* p;
    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
        if (!strcasecmp(p, "disc"))
            return (int)'d';
        else if (!strcasecmp(p, "circle"))
            return (int)'c';
        else if (!strcasecmp(p, "square"))
            return (int)'s';
    }
    return default_type;
}

int getMetaRefreshParam(const char* q, Str* refresh_uri)
{
    int refresh_interval;
    const char* r;
    Str s_tmp = NULL;

    if (q == NULL || refresh_uri == NULL)
        return 0;

    refresh_interval = atoi(q);
    if (refresh_interval < 0)
        return 0;

    while (*q) {
        if (!strncasecmp(q, "url=", 4)) {
            q += 4;
            if (*q == '\"' || *q == '\'') /* " or ' */
                q++;
            r = q;
            while (*r && !IS_SPACE(*r) && *r != ';')
                r++;
            s_tmp = Strnew_charp_n(q, r - q);

            if (s_tmp->length > 0 && (s_tmp->ptr[s_tmp->length - 1] == '\"' || /* " */
                    s_tmp->ptr[s_tmp->length - 1] == '\'')) { /* ' */
                s_tmp->length--;
                s_tmp->ptr[s_tmp->length] = '\0';
            }
            q = r;
        }
        while (*q && *q != ';')
            q++;
        if (*q == ';')
            q++;
        while (*q && *q == ' ')
            q++;
    }
    *refresh_uri = s_tmp;
    return refresh_interval;
}

int HTMLtagproc1(struct HtmlBuilder* hb, struct parsed_tag* tag, struct html_feed_environ* h_env)
{
    const char* p;
    const char* q;
    const char* r;
    int i, w, x, y, z, count, width;
    struct readbuffer* obuf = h_env->obuf;
    struct environment* envs = h_env->envs;
    Str tmp;
    int hseq;
    int cmd;
    char* id = NULL;

    cmd = tag->tagid;

    if (obuf->flag & RB_PRE) {
        switch (cmd) {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return 1;
        }
    }

    switch (cmd) {
    case HTML_B:
        if (obuf->in_bold < FONTSTAT_MAX)
            obuf->in_bold++;
        if (obuf->in_bold > 1)
            return 1;
        return 0;
    case HTML_N_B:
        if (obuf->in_bold == 1 && close_effect0(obuf, HTML_B))
            obuf->in_bold = 0;
        if (obuf->in_bold > 0) {
            obuf->in_bold--;
            if (obuf->in_bold == 0)
                return 0;
        }
        return 1;
    case HTML_I:
        if (obuf->in_italic < FONTSTAT_MAX)
            obuf->in_italic++;
        if (obuf->in_italic > 1)
            return 1;
        return 0;
    case HTML_N_I:
        if (obuf->in_italic == 1 && close_effect0(obuf, HTML_I))
            obuf->in_italic = 0;
        if (obuf->in_italic > 0) {
            obuf->in_italic--;
            if (obuf->in_italic == 0)
                return 0;
        }
        return 1;
    case HTML_U:
        if (obuf->in_under < FONTSTAT_MAX)
            obuf->in_under++;
        if (obuf->in_under > 1)
            return 1;
        return 0;
    case HTML_N_U:
        if (obuf->in_under == 1 && close_effect0(obuf, HTML_U))
            obuf->in_under = 0;
        if (obuf->in_under > 0) {
            obuf->in_under--;
            if (obuf->in_under == 0)
                return 0;
        }
        return 1;
    case HTML_EM:
        HTMLlineproc0(hb, "<i>", h_env, true);
        return 1;
    case HTML_N_EM:
        HTMLlineproc0(hb, "</i>", h_env, true);
        return 1;
    case HTML_STRONG:
        HTMLlineproc0(hb, "<b>", h_env, true);
        return 1;
    case HTML_N_STRONG:
        HTMLlineproc0(hb, "</b>", h_env, true);
        return 1;
    case HTML_Q:
        if (getRuntime()->DisplayCharset != WC_CES_US_ASCII) {
            HTMLlineproc0(hb, (obuf->q_level & 1 ? "&lsquo;" : "&ldquo;"), h_env, true);
            obuf->q_level += 1;
        } else

            HTMLlineproc0(hb, "`", h_env, true);
        return 1;
    case HTML_N_Q:
        if (getRuntime()->DisplayCharset != WC_CES_US_ASCII) {
            obuf->q_level -= 1;
            HTMLlineproc0(hb, (obuf->q_level & 1 ? "&rsquo;" : "&rdquo;"), h_env, true);
        } else

            HTMLlineproc0(hb, "'", h_env, true);
        return 1;
    case HTML_FIGURE:
    case HTML_N_FIGURE:
    case HTML_P:
    case HTML_N_P:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        obuf->flag |= RB_IGNORE_P;
        if (cmd == HTML_P) {
            set_alignment(obuf, tag);
            obuf->flag |= RB_P;
        }
        return 1;
    case HTML_FIGCAPTION:
    case HTML_N_FIGCAPTION:
    case HTML_BR:
        flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
        h_env->blank_lines = 0;
        return 1;
    case HTML_H:
        if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P))) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        HTMLlineproc0(hb, "<b>", h_env, true);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_H:
        HTMLlineproc0(hb, "</b>", h_env, true);
        if (!(obuf->flag & RB_PREMODE)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        }
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        close_anchor(hb, h_env, obuf);
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_UL:
    case HTML_OL:
    case HTML_BLQ:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            if (!(obuf->flag & RB_PREMODE) && (h_env->envc == 0 || cmd == HTML_BLQ))
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
        }
        PUSH_ENV(cmd);
        if (cmd == HTML_UL || cmd == HTML_OL) {
            if (parsedtag_get_value(tag, ATTR_START, &count)) {
                envs[h_env->envc].count = count - 1;
            }
        }
        if (cmd == HTML_OL) {
            envs[h_env->envc].type = '1';
            if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
                envs[h_env->envc].type = (int)*p;
            }
        }
        if (cmd == HTML_UL)
            envs[h_env->envc].type = ul_type(tag, 0);
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        return 1;
    case HTML_N_UL:
    case HTML_N_OL:
    case HTML_N_DL:
    case HTML_N_BLQ:
    case HTML_N_DD:
        CLOSE_DT;
        CLOSE_A;
        if (h_env->envc > 0) {
            flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0,
                h_env->limit);
            POP_ENV;
            if (!(obuf->flag & RB_PREMODE) && (h_env->envc == 0 || cmd == HTML_N_BLQ)) {
                do_blankline(h_env, obuf,
                    envs[h_env->envc].indent,
                    getRuntime()->IndentIncr, h_env->limit);
                obuf->flag |= RB_IGNORE_P;
            }
        }
        close_anchor(hb, h_env, obuf);
        return 1;
    case HTML_DL:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            if (!(obuf->flag & RB_PREMODE) && envs[h_env->envc].env != HTML_DL
                && envs[h_env->envc].env != HTML_DL_COMPACT
                && envs[h_env->envc].env != HTML_DD)
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
        }
        PUSH_ENV_NOINDENT(cmd);
        if (parsedtag_exists(tag, ATTR_COMPACT))
            envs[h_env->envc].env = HTML_DL_COMPACT;
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_LI:
        CLOSE_A;
        CLOSE_DT;
        if (h_env->envc > 0) {
            Str num;
            flushline(h_env, obuf,
                envs[h_env->envc - 1].indent, 0, h_env->limit);
            envs[h_env->envc].count++;
            if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
                count = atoi(p);
                if (count > 0)
                    envs[h_env->envc].count = count;
                else
                    envs[h_env->envc].count = 0;
            }
            switch (envs[h_env->envc].env) {
            case HTML_UL:
                envs[h_env->envc].type = ul_type(tag, envs[h_env->envc].type);
                for (i = 0; i < getRuntime()->IndentIncr - 3; i++)
                    push_charp(obuf, 1, NBSP, PC_ASCII);
                tmp = Strnew();
                switch (envs[h_env->envc].type) {
                case 'd':
                    push_symbol(tmp, UL_SYMBOL_DISC, symbol_width, 1);
                    break;
                case 'c':
                    push_symbol(tmp, UL_SYMBOL_CIRCLE, symbol_width, 1);
                    break;
                case 's':
                    push_symbol(tmp, UL_SYMBOL_SQUARE, symbol_width, 1);
                    break;
                default:
                    push_symbol(tmp,
                        UL_SYMBOL((h_env->envc_real - 1) % MAX_UL_LEVEL), symbol_width,
                        1);
                    break;
                }
                if (symbol_width == 1)
                    push_charp(obuf, 1, NBSP, PC_ASCII);
                push_str(obuf, symbol_width, tmp, PC_ASCII);
                push_charp(obuf, 1, NBSP, PC_ASCII);
                Strcopy_charp_n(obuf->prevchar, " ", 1);
                break;
            case HTML_OL:
                if (parsedtag_get_value(tag, ATTR_TYPE, &p))
                    envs[h_env->envc].type = (int)*p;
                switch ((envs[h_env->envc].count > 0) ? envs[h_env->envc].type : '1') {
                case 'i':
                    num = romanNumeral(envs[h_env->envc].count);
                    break;
                case 'I':
                    num = romanNumeral(envs[h_env->envc].count);
                    Strupper(num);
                    break;
                case 'a':
                    num = romanAlphabet(envs[h_env->envc].count);
                    break;
                case 'A':
                    num = romanAlphabet(envs[h_env->envc].count);
                    Strupper(num);
                    break;
                default:
                    num = Sprintf("%d", envs[h_env->envc].count);
                    break;
                }
                if (getRuntime()->IndentIncr >= 4)
                    Strcat_charp(num, ". ");
                else
                    Strcat_char(num, '.');
                push_spaces(obuf, 1, getRuntime()->IndentIncr - num->length);
                push_str(obuf, num->length, num, PC_ASCII);
                if (getRuntime()->IndentIncr >= 4)
                    Strcopy_charp_n(obuf->prevchar, " ", 1);
                break;
            default:
                push_spaces(obuf, 1, getRuntime()->IndentIncr);
                break;
            }
        } else {
            flushline(h_env, obuf, 0, 0, h_env->limit);
        }
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_DT:
        CLOSE_A;
        if (h_env->envc == 0 || (h_env->envc_real < h_env->nenv && envs[h_env->envc].env != HTML_DL && envs[h_env->envc].env != HTML_DL_COMPACT)) {
            PUSH_ENV_NOINDENT(HTML_DL);
        }
        if (h_env->envc > 0) {
            flushline(h_env, obuf,
                envs[h_env->envc - 1].indent, 0, h_env->limit);
        }
        if (!(obuf->flag & RB_IN_DT)) {
            HTMLlineproc0(hb, "<b>", h_env, true);
            obuf->flag |= RB_IN_DT;
        }
        obuf->flag |= RB_IGNORE_P;
        return 1;
    case HTML_N_DT:
        if (!(obuf->flag & RB_IN_DT)) {
            return 1;
        }
        obuf->flag &= ~RB_IN_DT;
        HTMLlineproc0(hb, "</b>", h_env, true);
        if (h_env->envc > 0 && envs[h_env->envc].env == HTML_DL)
            flushline(h_env, obuf,
                envs[h_env->envc - 1].indent, 0, h_env->limit);
        return 1;
    case HTML_DD:
        CLOSE_A;
        CLOSE_DT;
        if (envs[h_env->envc].env == HTML_DL || envs[h_env->envc].env == HTML_DL_COMPACT) {
            PUSH_ENV(HTML_DD);
        }

        if (h_env->envc > 0 && envs[h_env->envc - 1].env == HTML_DL_COMPACT) {
            if (obuf->pos > envs[h_env->envc].indent)
                flushline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
            else
                push_spaces(obuf, 1, envs[h_env->envc].indent - obuf->pos);
        } else
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        /* obuf->flag |= RB_IGNORE_P; */
        return 1;
    case HTML_TITLE:
        close_anchor(hb, h_env, obuf);
        process_title(hb, tag);
        obuf->flag |= RB_TITLE;
        obuf->end_tag = HTML_N_TITLE;
        return 1;
    case HTML_N_TITLE:
        if (!(obuf->flag & RB_TITLE))
            return 1;
        obuf->flag &= ~RB_TITLE;
        obuf->end_tag = 0;
        tmp = process_n_title(hb, tag);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_TITLE_ALT:
        if (parsedtag_get_value(tag, ATTR_TITLE, &p))
            h_env->title = html_unquote(p);
        return 0;
    case HTML_FRAMESET:
        PUSH_ENV(cmd);
        push_charp(obuf, 9, "--FRAME--", PC_ASCII);
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        return 0;
    case HTML_N_FRAMESET:
        if (h_env->envc > 0) {
            POP_ENV;
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        }
        return 0;
    case HTML_NOFRAMES:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    case HTML_N_NOFRAMES:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        obuf->flag &= ~RB_NOFRAMES;
        return 1;
    case HTML_FRAME:
        q = r = NULL;
        parsedtag_get_value(tag, ATTR_SRC, &q);
        parsedtag_get_value(tag, ATTR_NAME, &r);
        if (q) {
            q = html_quote(q);
            push_tag(obuf, Sprintf("<a hseq=\"%d\" href=\"%s\">", hb->cur_hseq++, q)->ptr, HTML_A);
            if (r)
                q = html_quote(r);
            push_charp(obuf, get_strwidth(q), q, PC_ASCII);
            push_tag(obuf, "</a>", HTML_N_A);
        }
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        return 0;
    case HTML_HR:
        close_anchor(hb, h_env, obuf);
        tmp = process_hr(tag, h_env->limit, envs[h_env->envc].indent);
        HTMLlineproc0(hb, tmp->ptr, h_env, true);
        Strcopy_charp_n(obuf->prevchar, " ", 1);
        return 1;
    case HTML_PRE:
        x = parsedtag_exists(tag, ATTR_FOR_TABLE);
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            if (!x)
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
        } else
            fillline(obuf, envs[h_env->envc].indent);
        obuf->flag |= (RB_PRE | RB_IGNORE_P);
        /* istr = str; */
        return 1;
    case HTML_N_PRE:
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        if (!(obuf->flag & RB_IGNORE_P)) {
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
            obuf->flag |= RB_IGNORE_P;
            h_env->blank_lines++;
        }
        obuf->flag &= ~RB_PRE;
        close_anchor(hb, h_env, obuf);
        return 1;
    case HTML_PRE_INT:
        i = obuf->line->length;
        append_tags(obuf);
        if (!(obuf->flag & RB_SPECIAL)) {
            set_breakpoint(obuf, obuf->line->length - i);
        }
        obuf->flag |= RB_PRE_INT;
        return 0;
    case HTML_N_PRE_INT:
        push_tag(obuf, "</pre_int>", HTML_N_PRE_INT);
        obuf->flag &= ~RB_PRE_INT;
        if (!(obuf->flag & RB_SPECIAL) && obuf->pos > obuf->bp.pos) {
            Strcopy_charp_n(obuf->prevchar, "", 0);
            obuf->prev_ctype = PC_CTRL;
        }
        return 1;
    case HTML_NOBR:
        obuf->flag |= RB_NOBR;
        obuf->nobr_level++;
        return 0;
    case HTML_N_NOBR:
        if (obuf->nobr_level > 0)
            obuf->nobr_level--;
        if (obuf->nobr_level == 0)
            obuf->flag &= ~RB_NOBR;
        return 0;
    case HTML_PRE_PLAIN:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        obuf->flag |= (RB_PRE | RB_IGNORE_P);
        return 1;
    case HTML_N_PRE_PLAIN:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
            obuf->flag |= RB_IGNORE_P;
        }
        obuf->flag &= ~RB_PRE;
        return 1;
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
        }
        obuf->flag |= (RB_PLAIN | RB_IGNORE_P);
        switch (cmd) {
        case HTML_LISTING:
            obuf->end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            obuf->end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            obuf->end_tag = MAX_HTMLTAG;
            break;
        }
        return 1;
    case HTML_N_LISTING:
    case HTML_N_XMP:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P)) {
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
            do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                h_env->limit);
            obuf->flag |= RB_IGNORE_P;
        }
        obuf->flag &= ~RB_PLAIN;
        obuf->end_tag = 0;
        return 1;
    case HTML_SCRIPT:
        obuf->flag |= RB_SCRIPT;
        obuf->end_tag = HTML_N_SCRIPT;
        return 1;
    case HTML_STYLE:
        obuf->flag |= RB_STYLE;
        obuf->end_tag = HTML_N_STYLE;
        return 1;
    case HTML_N_SCRIPT:
        obuf->flag &= ~RB_SCRIPT;
        obuf->end_tag = 0;
        return 1;
    case HTML_N_STYLE:
        obuf->flag &= ~RB_STYLE;
        obuf->end_tag = 0;
        return 1;
    case HTML_A:
        if (obuf->anchor.url)
            close_anchor(hb, h_env, obuf);

        hseq = 0;

        if (parsedtag_get_value(tag, ATTR_HREF, &p))
            obuf->anchor.url = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_TARGET, &p))
            obuf->anchor.target = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_REFERER, &p))
            obuf->anchor.referer = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_TITLE, &p))
            obuf->anchor.title = Strnew_charp(p)->ptr;
        if (parsedtag_get_value(tag, ATTR_ACCESSKEY, &p))
            obuf->anchor.accesskey = (unsigned char)*p;
        if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq))
            obuf->anchor.hseq = hseq;

        if (hseq == 0 && obuf->anchor.url) {
            obuf->anchor.hseq = hb->cur_hseq;
            tmp = process_anchor(hb, tag, h_env->tagbuf->ptr);
            push_tag(obuf, tmp->ptr, HTML_A);
            return 1;
        }
        return 0;
    case HTML_N_A:
        close_anchor(hb, h_env, obuf);
        return 1;
    case HTML_IMG:
        if (parsedtag_exists(tag, ATTR_USEMAP))
            HTML5_CLOSE_A;
        tmp = process_img(hb, tag, h_env->limit);
        if (need_number) {
            tmp = Strnew_m_charp(getLinkNumberStr(hb, -1)->ptr, tmp->ptr, NULL);
            need_number = 0;
        }
        HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_IMG_ALT:
        if (parsedtag_get_value(tag, ATTR_SRC, &p))
            obuf->img_alt = Strnew_charp(p);
#ifdef USE_IMAGE
        i = 0;
        if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
            if ((short)i > obuf->top_margin)
                obuf->top_margin = (short)i;
        }
        i = 0;
        if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
            if ((short)i > obuf->bottom_margin)
                obuf->bottom_margin = (short)i;
        }
#endif
        return 0;
    case HTML_N_IMG_ALT:
        if (obuf->img_alt) {
            if (!close_effect0(obuf, HTML_IMG_ALT))
                push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
            obuf->img_alt = NULL;
        }
        return 1;
    case HTML_INPUT_ALT:
        i = 0;
        if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
            if ((short)i > obuf->top_margin)
                obuf->top_margin = (short)i;
        }
        i = 0;
        if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
            if ((short)i > obuf->bottom_margin)
                obuf->bottom_margin = (short)i;
        }
        if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq)) {
            obuf->input_alt.hseq = hseq;
        }
        if (parsedtag_get_value(tag, ATTR_FID, &i)) {
            obuf->input_alt.fid = i;
        }
        if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
            obuf->input_alt.type = Strnew_charp(p);
        }
        if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
            obuf->input_alt.value = Strnew_charp(p);
        }
        if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
            obuf->input_alt.name = Strnew_charp(p);
        }
        obuf->input_alt.in = 1;
        return 0;
    case HTML_N_INPUT_ALT:
        if (obuf->input_alt.in) {
            if (!close_effect0(obuf, HTML_INPUT_ALT))
                push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
            obuf->input_alt.hseq = 0;
            obuf->input_alt.fid = -1;
            obuf->input_alt.in = 0;
            obuf->input_alt.type = NULL;
            obuf->input_alt.name = NULL;
            obuf->input_alt.value = NULL;
        }
        return 1;
    case HTML_TABLE:
        close_anchor(hb, h_env, obuf);
        if (obuf->table_level + 1 >= MAX_TABLE)
            break;
        obuf->table_level++;
        w = BORDER_NONE;
        /* x: cellspacing, y: cellpadding */
        x = 2;
        y = 1;
        z = 0;
        width = 0;
        if (parsedtag_exists(tag, ATTR_BORDER)) {
            if (parsedtag_get_value(tag, ATTR_BORDER, &w)) {
                if (w > 2)
                    w = BORDER_THICK;
                else if (w < 0) { /* weird */
                    w = BORDER_THIN;
                }
            } else
                w = BORDER_THIN;
        }
        if (getRuntime()->DisplayBorders && w == BORDER_NONE)
            w = BORDER_THIN;
        if (parsedtag_get_value(tag, ATTR_WIDTH, &i)) {
            if (obuf->table_level == 0)
                width = REAL_WIDTH(i, h_env->limit - envs[h_env->envc].indent);
            else
                width = RELATIVE_WIDTH(i);
        }
        if (parsedtag_exists(tag, ATTR_HBORDER))
            w = BORDER_NOWIN;
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000
        parsedtag_get_value(tag, ATTR_CELLSPACING, &x);
        parsedtag_get_value(tag, ATTR_CELLPADDING, &y);
        parsedtag_get_value(tag, ATTR_VSPACE, &z);
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (z < 0)
            z = 0;
        if (x > MAX_CELLSPACING)
            x = MAX_CELLSPACING;
        if (y > MAX_CELLPADDING)
            y = MAX_CELLPADDING;
        if (z > MAX_VSPACE)
            z = MAX_VSPACE;
#ifdef ID_EXT
        parsedtag_get_value(tag, ATTR_ID, &id);
#endif /* ID_EXT */
        hb->tables[obuf->table_level] = begin_table(w, x, y, z);
#ifdef ID_EXT
        if (id != NULL)
            hb->tables[obuf->table_level]->id = Strnew_charp(id);
#endif /* ID_EXT */
        hb->table_mode[obuf->table_level].pre_mode = 0;
        hb->table_mode[obuf->table_level].indent_level = 0;
        hb->table_mode[obuf->table_level].nobr_level = 0;
        hb->table_mode[obuf->table_level].caption = 0;
        hb->table_mode[obuf->table_level].end_tag = 0; /* HTML_UNKNOWN */
#ifndef TABLE_EXPAND
        hb->tables[obuf->table_level]->total_width = width;
#else
        tables[obuf->table_level]->real_width = width;
        tables[obuf->table_level]->total_width = 0;
#endif
        return 1;
    case HTML_N_TABLE:
        /* should be processed in HTMLlineproc() */
        return 1;
    case HTML_CENTER:
        CLOSE_A;
        if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_SAVE_FLAG(obuf);
        if (getRuntime()->DisableCenter)
            RB_SET_ALIGN(obuf, RB_LEFT);
        else
            RB_SET_ALIGN(obuf, RB_CENTER);
        return 1;
    case HTML_N_CENTER:
        CLOSE_A;
        if (!(obuf->flag & RB_PREMODE))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_DIV:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_DIV:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_DIV_INT:
        CLOSE_P;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        set_alignment(obuf, tag);
        return 1;
    case HTML_N_DIV_INT:
        CLOSE_P;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        RB_RESTORE_FLAG(obuf);
        return 1;
    case HTML_FORM:
        CLOSE_A;
        if (!(obuf->flag & RB_IGNORE_P))
            flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        tmp = process_form(hb, tag);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_N_FORM:
        CLOSE_A;
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        obuf->flag |= RB_IGNORE_P;
        process_n_form(hb);
        return 1;
    case HTML_INPUT:
        close_anchor(hb, h_env, obuf);
        tmp = process_input(hb, tag);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_BUTTON:
        HTML5_CLOSE_A;
        tmp = process_button(hb, tag);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_N_BUTTON:
        tmp = process_n_button();
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_SELECT:
        close_anchor(hb, h_env, obuf);
        tmp = process_select(hb, tag);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        obuf->flag |= RB_INSELECT;
        obuf->end_tag = HTML_N_SELECT;
        return 1;
    case HTML_N_SELECT:
        obuf->flag &= ~RB_INSELECT;
        obuf->end_tag = 0;
        tmp = process_n_select(hb);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_OPTION:
        /* nothing */
        return 1;
    case HTML_TEXTAREA:
        close_anchor(hb, h_env, obuf);
        tmp = process_textarea(hb, tag, h_env->limit);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        obuf->flag |= RB_INTXTA;
        obuf->end_tag = HTML_N_TEXTAREA;
        return 1;
    case HTML_N_TEXTAREA:
        obuf->flag &= ~RB_INTXTA;
        obuf->end_tag = 0;
        tmp = process_n_textarea(hb);
        if (tmp)
            HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_ISINDEX:
        p = "";
        q = "!CURRENT_URL!";
        parsedtag_get_value(tag, ATTR_PROMPT, &p);
        parsedtag_get_value(tag, ATTR_ACTION, &q);
        tmp = Strnew_m_charp("<form method=get action=\"",
            html_quote(q),
            "\">",
            html_quote(p),
            "<input type=text name=\"\" accept></form>",
            NULL);
        HTMLlineproc0(hb, tmp->ptr, h_env, true);
        return 1;
    case HTML_DOCTYPE:
        if (!parsedtag_exists(tag, ATTR_PUBLIC)) {
            obuf->flag |= RB_HTML5;
        }
        return 1;
    case HTML_META:
        p = q = r = NULL;
        parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
        parsedtag_get_value(tag, ATTR_CONTENT, &q);

        parsedtag_get_value(tag, ATTR_CHARSET, &r);
        if (r) {
            /* <meta charset=""> */
            r = skip_blanks(r);
            hb->meta_charset = wc_guess_charset(r, 0);
        } else if (p && q && !strcasecmp(p, "Content-Type") && (q = strcasestr(q, "charset")) != NULL) {
            q += 7;
            q = skip_blanks(q);
            if (*q == '=') {
                q++;
                q = skip_blanks(q);
                hb->meta_charset = wc_guess_charset(q, 0);
            }
        } else

            if (p && q && !strcasecmp(p, "refresh")) {
            int refresh_interval;
            tmp = NULL;
            refresh_interval = getMetaRefreshParam(q, &tmp);
            if (tmp) {
                q = html_quote(tmp->ptr);
                tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
                    refresh_interval, q, q);
            } else if (refresh_interval > 0)
                tmp = Sprintf("Refresh (%d sec)", refresh_interval);
            if (tmp) {
                HTMLlineproc0(hb, tmp->ptr, h_env, true);
                do_blankline(h_env, obuf, envs[h_env->envc].indent, 0,
                    h_env->limit);
                if (!getRuntime()->is_redisplay && !((obuf->flag & RB_NOFRAMES) && getRuntime()->RenderFrame)) {
                    tag->need_reconstruct = TRUE;
                    return 0;
                }
            }
        }
        return 1;
    case HTML_BASE:
        p = NULL;
        if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
            hb->cur_baseURL = New(struct Url);
            parseURL(p, hb->cur_baseURL, NULL);
        }
    case HTML_MAP:
    case HTML_N_MAP:
    case HTML_AREA:
        return 0;
    case HTML_DEL:
        switch (getRuntime()->displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag |= RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0(hb, "<U>[DEL:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike < FONTSTAT_MAX)
                obuf->in_strike++;
            if (obuf->in_strike == 1) {
                push_tag(obuf, "<s>", HTML_S);
            }
            break;
        }
        return 1;
    case HTML_N_DEL:
        switch (getRuntime()->displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag &= ~RB_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0(hb, "<U>:DEL]</U>", h_env, true);
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike == 0)
                return 1;
            if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
                obuf->in_strike = 0;
            if (obuf->in_strike > 0) {
                obuf->in_strike--;
                if (obuf->in_strike == 0) {
                    push_tag(obuf, "</s>", HTML_N_S);
                }
            }
            break;
        }
        return 1;
    case HTML_S:
        switch (getRuntime()->displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag |= RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0(hb, "<U>[S:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike < FONTSTAT_MAX)
                obuf->in_strike++;
            if (obuf->in_strike == 1) {
                push_tag(obuf, "<s>", HTML_S);
            }
            break;
        }
        return 1;
    case HTML_N_S:
        switch (getRuntime()->displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            obuf->flag &= ~RB_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0(hb, "<U>:S]</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_strike == 0)
                return 1;
            if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
                obuf->in_strike = 0;
            if (obuf->in_strike > 0) {
                obuf->in_strike--;
                if (obuf->in_strike == 0) {
                    push_tag(obuf, "</s>", HTML_N_S);
                }
            }
        }
        return 1;
    case HTML_INS:
        switch (getRuntime()->displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0(hb, "<U>[INS:</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_ins < FONTSTAT_MAX)
                obuf->in_ins++;
            if (obuf->in_ins == 1) {
                push_tag(obuf, "<ins>", HTML_INS);
            }
            break;
        }
        return 1;
    case HTML_N_INS:
        switch (getRuntime()->displayInsDel) {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            HTMLlineproc0(hb, "<U>:INS]</U>", h_env, true);
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            if (obuf->in_ins == 0)
                return 1;
            if (obuf->in_ins == 1 && close_effect0(obuf, HTML_INS))
                obuf->in_ins = 0;
            if (obuf->in_ins > 0) {
                obuf->in_ins--;
                if (obuf->in_ins == 0) {
                    push_tag(obuf, "</ins>", HTML_N_INS);
                }
            }
            break;
        }
        return 1;
    case HTML_SUP:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0(hb, "^", h_env, true);
        return 1;
    case HTML_N_SUP:
        return 1;
    case HTML_SUB:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0(hb, "[", h_env, true);
        return 1;
    case HTML_N_SUB:
        if (!(obuf->flag & (RB_DEL | RB_S)))
            HTMLlineproc0(hb, "]", h_env, true);
        return 1;
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        return 1;
    case HTML_BGSOUND:
        if (getRuntime()->view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
                HTMLlineproc0(hb, s->ptr, h_env, true);
            }
        }
        return 1;
    case HTML_EMBED:
        HTML5_CLOSE_A;
        if (getRuntime()->view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
                HTMLlineproc0(hb, s->ptr, h_env, true);
            }
        }
        return 1;
    case HTML_APPLET:
        if (getRuntime()->view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_ARCHIVE, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
                HTMLlineproc0(hb, s->ptr, h_env, true);
            }
        }
        return 1;
    case HTML_BODY:
        if (getRuntime()->view_unseenobject) {
            if (parsedtag_get_value(tag, ATTR_BACKGROUND, &p)) {
                Str s;
                q = html_quote(p);
                s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
                HTMLlineproc0(hb, s->ptr, h_env, true);
            }
        }
    case HTML_N_HEAD:
        if (obuf->flag & RB_TITLE)
            HTMLlineproc0(hb, "</title>", h_env, true);
    case HTML_HEAD:
    case HTML_N_BODY:
        return 1;
    default:
        /* obuf->prevchar = '\0'; */
        return 0;
    }
    /* not reached */
    return 0;
}

#define PPUSH(p, c)      \
    {                    \
        outp[pos] = (p); \
        outc[pos] = (c); \
        pos++;           \
    }
#define PSIZE                                       \
    if (out_size <= pos + 1) {                      \
        out_size = pos * 3 / 2;                     \
        outc = New_Reuse(char, outc, out_size);     \
        outp = New_Reuse(Lineprop, outp, out_size); \
    }

static TextLineListItem* _tl_lp2;

static Str
textlist_feed(void)
{
    TextLine* p;
    if (_tl_lp2 != NULL) {
        p = _tl_lp2->ptr;
        _tl_lp2 = _tl_lp2->next;
        return p->line;
    }
    return NULL;
}

static int
ex_efct(int ex)
{
    int effect = 0;

    if (!ex)
        return 0;

    if (ex & PE_EX_ITALIC)
        effect |= PE_EX_ITALIC_E;

    if (ex & PE_EX_INSERT)
        effect |= PE_EX_INSERT_E;

    if (ex & PE_EX_STRIKE)
        effect |= PE_EX_STRIKE_E;

    return effect;
}

static void
addLink(struct Buffer* buf, struct parsed_tag* tag)
{
    char *href = NULL, *title = NULL, *ctype = NULL, *rel = NULL, *rev = NULL;
    char type = LINK_TYPE_NONE;
    struct LinkList* l;

    parsedtag_get_value(tag, ATTR_HREF, &href);
    if (href)
        href = url_encode(remove_space(href), baseURL(buf),
            buf->document_charset);
    parsedtag_get_value(tag, ATTR_TITLE, &title);
    parsedtag_get_value(tag, ATTR_TYPE, &ctype);
    parsedtag_get_value(tag, ATTR_REL, &rel);
    if (rel != NULL) {
        /* forward link type */
        type = LINK_TYPE_REL;
        if (title == NULL)
            title = rel;
    }
    parsedtag_get_value(tag, ATTR_REV, &rev);
    if (rev != NULL) {
        /* reverse link type */
        type = LINK_TYPE_REV;
        if (title == NULL)
            title = rev;
    }

    l = New(struct LinkList);
    l->url = href;
    l->title = title;
    l->ctype = ctype;
    l->type = type;
    l->next = NULL;
    if (buf->linklist) {
        struct LinkList* i;
        for (i = buf->linklist; i->next; i = i->next)
            ;
        i->next = l;
    } else
        buf->linklist = l;
}

static void
HTMLlineproc2body(struct HtmlBuilder* hb, struct Buffer* buf, Str (*feed)(), int llimit)
{
    static char* outc = NULL;
    static Lineprop* outp = NULL;
    static int out_size = 0;
    struct Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
    const char *p, *q, *r, *s, *t, *str;
    Lineprop mode, effect, ex_effect;
    int pos;
    int nlines;
    struct frameset* frameset_s[FRAMESTACK_SIZE];
    int frameset_sp = -1;
    union frameset_element* idFrame = NULL;
    char* id = NULL;
    int hseq, form_id;
    Str line;
    const char* endp;
    char symbol = '\0';
    int internal = 0;
    struct Anchor** a_textarea = NULL;
#ifdef MENU_SELECT
    struct Anchor** a_select = NULL;
#endif
#if defined(USE_M17N) || defined(USE_IMAGE)
    struct Url* base = baseURL(buf);
#endif
#ifdef USE_M17N
    wc_ces name_charset = url_to_charset(NULL, &buf->currentURL,
        buf->document_charset);
#endif

    if (out_size == 0) {
        out_size = LINELEN;
        outc = NewAtom_N(char, out_size);
        outp = NewAtom_N(Lineprop, out_size);
    }

    hb->n_textarea = -1;
    if (!hb->max_textarea) { /* halfload */
        hb->max_textarea = MAX_TEXTAREA;
        hb->textarea_str = New_N(Str, hb->max_textarea);
        a_textarea = New_N(struct Anchor*, hb->max_textarea);
    }

    hb->n_select = -1;
    if (!hb->max_select) { /* halfload */
        hb->max_select = MAX_SELECT;
        hb->select_option = New_N(struct FormSelectOption, hb->max_select);
        a_select = New_N(struct Anchor*, hb->max_select);
    }

    effect = 0;
    ex_effect = 0;
    nlines = 0;
    while ((line = feed()) != NULL) {
        if (hb->n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
            Strcat(hb->textarea_str[hb->n_textarea], line);
            continue;
        }
    proc_again:
        if (++nlines == llimit)
            break;
        pos = 0;

        Strremovetrailingspaces(line);

        str = line->ptr;
        endp = str + line->length;
        while (str < endp) {
            PSIZE;
            mode = get_mctype(str);
            if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<') {
                char** buf = set_symbol(symbol_width0);
                int len;

                p = buf[(int)symbol];
                len = get_mclen(p);
                mode = get_mctype(p);
                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                if (--len) {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--) {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                    }
                }
                str += symbol_width;
            } else if (mode == PC_CTRL || mode == PC_UNDEF) {
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str++;
            } else if (mode & PC_UNKNOWN) {
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str += get_mclen(str);
            } else if (*str != '<' && *str != '&') {
                int len = get_mclen(str);
                PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
                if (--len) {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--) {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
                    }
                }
            } else if (*str == '&') {
                /*
                 * & escape processing
                 */
                p = getescapecmd(&str);
                while (*p) {
                    PSIZE;
                    mode = get_mctype(p);
                    if (mode == PC_CTRL || mode == PC_UNDEF) {
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p++;
                    } else if (mode & PC_UNKNOWN) {
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p += get_mclen(p);
                    } else {
                        int len = get_mclen(p);
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                        if (--len) {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--) {
                                PSIZE;
                                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                            }
                        }
                    }
                }
            } else {
                /* tag processing */
                struct parsed_tag* tag;
                if (!(tag = parse_tag(&str, TRUE)))
                    continue;
                switch (tag->tagid) {
                case HTML_B:
                    effect |= PE_BOLD;
                    break;
                case HTML_N_B:
                    effect &= ~PE_BOLD;
                    break;
                case HTML_I:
                    ex_effect |= PE_EX_ITALIC;
                    break;
                case HTML_N_I:
                    ex_effect &= ~PE_EX_ITALIC;
                    break;
                case HTML_INS:
                    ex_effect |= PE_EX_INSERT;
                    break;
                case HTML_N_INS:
                    ex_effect &= ~PE_EX_INSERT;
                    break;
                case HTML_U:
                    effect |= PE_UNDER;
                    break;
                case HTML_N_U:
                    effect &= ~PE_UNDER;
                    break;
                case HTML_S:
                    ex_effect |= PE_EX_STRIKE;
                    break;
                case HTML_N_S:
                    ex_effect &= ~PE_EX_STRIKE;
                    break;
                case HTML_A:
                    if (renderFrameSet && parsedtag_get_value(tag, ATTR_FRAMENAME, &p)) {
                        p = url_quote_conv(p, buf->document_charset);
                        if (!idFrame || strcmp(idFrame->body->name, p)) {
                            idFrame = search_frame(renderFrameSet, p);
                            if (idFrame && idFrame->body->attr != F_BODY)
                                idFrame = NULL;
                        }
                    }
                    p = r = s = NULL;
                    q = buf->baseTarget;
                    t = "";
                    hseq = 0;
                    id = NULL;
                    if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
                        id = url_quote_conv(id, name_charset);
                        registerName(buf, id, currentLn(buf), pos);
                    }
                    if (parsedtag_get_value(tag, ATTR_HREF, &p))
                        p = url_encode(remove_space(p), base,
                            buf->document_charset);
                    if (parsedtag_get_value(tag, ATTR_TARGET, &q))
                        q = url_quote_conv(q, buf->document_charset);
                    if (parsedtag_get_value(tag, ATTR_REFERER, &r))
                        r = url_encode(r, base,
                            buf->document_charset);
                    parsedtag_get_value(tag, ATTR_TITLE, &s);
                    parsedtag_get_value(tag, ATTR_ACCESSKEY, &t);
                    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
                    if (hseq > 0)
                        buf->hmarklist = putHmarker(buf->hmarklist, currentLn(buf),
                            pos, hseq - 1);
                    else if (hseq < 0) {
                        int h = -hseq - 1;
                        if (buf->hmarklist && h < buf->hmarklist->nmark && buf->hmarklist->marks[h].invalid) {
                            buf->hmarklist->marks[h].pos = pos;
                            buf->hmarklist->marks[h].line = currentLn(buf);
                            buf->hmarklist->marks[h].invalid = 0;
                            hseq = -hseq;
                        }
                    }
                    if (id && idFrame)
                        idFrame->body->nameList = putAnchor(idFrame->body->nameList, id, NULL,
                            (struct Anchor**)NULL, NULL, NULL, '\0',
                            currentLn(buf), pos);
                    if (p) {
                        effect |= PE_ANCHOR;
                        a_href = registerHref(buf, p, q, r, s,
                            *t, currentLn(buf), pos);
                        a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
                        a_href->slave = (hseq > 0) ? FALSE : TRUE;
                    }
                    break;
                case HTML_N_A:
                    effect &= ~PE_ANCHOR;
                    if (a_href) {
                        a_href->end.line = currentLn(buf);
                        a_href->end.pos = pos;
                        if (a_href->start.line == a_href->end.line && a_href->start.pos == a_href->end.pos) {
                            if (buf->hmarklist && a_href->hseq >= 0 && a_href->hseq < buf->hmarklist->nmark)
                                buf->hmarklist->marks[a_href->hseq].invalid = 1;
                            a_href->hseq = -1;
                        }
                        a_href = NULL;
                    }
                    break;

                case HTML_LINK:
                    addLink(buf, tag);
                    break;

                case HTML_IMG_ALT:
                    if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
#ifdef USE_IMAGE
                        int w = -1, h = -1, iseq = 0, ismap = 0;
                        int xoffset = 0, yoffset = 0, top = 0, bottom = 0;
                        parsedtag_get_value(tag, ATTR_HSEQ, &iseq);
                        parsedtag_get_value(tag, ATTR_WIDTH, &w);
                        parsedtag_get_value(tag, ATTR_HEIGHT, &h);
                        parsedtag_get_value(tag, ATTR_XOFFSET, &xoffset);
                        parsedtag_get_value(tag, ATTR_YOFFSET, &yoffset);
                        parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
                        parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
                        if (parsedtag_exists(tag, ATTR_ISMAP))
                            ismap = 1;
                        q = NULL;
                        parsedtag_get_value(tag, ATTR_USEMAP, &q);
                        if (iseq > 0) {
                            buf->imarklist = putHmarker(buf->imarklist,
                                currentLn(buf), pos,
                                iseq - 1);
                        }
#endif
                        s = NULL;
                        parsedtag_get_value(tag, ATTR_TITLE, &s);
                        p = url_quote_conv(remove_space(p),
                            buf->document_charset);
                        a_img = registerImg(buf, p, s, currentLn(buf), pos);
#ifdef USE_IMAGE
                        a_img->hseq = iseq;
                        a_img->image = NULL;
                        if (iseq > 0) {
                            struct Url u;
                            struct Image* image;

                            parseURL2(a_img->url, &u, base);
                            a_img->image = image = New(struct Image);
                            image->url = parsedURL2Str(&u)->ptr;
                            if (!uncompressed_file_type(u.file, &image->ext))
                                image->ext = filename_extension(u.file, TRUE);
                            image->cache = NULL;
                            image->width = (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
                            image->height = (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
                            image->xoffset = xoffset;
                            image->yoffset = yoffset;
                            image->y = currentLn(buf) - top;
                            if (image->xoffset < 0 && pos == 0)
                                image->xoffset = 0;
                            if (image->yoffset < 0 && image->y == 1)
                                image->yoffset = 0;
                            image->rows = 1 + top + bottom;
                            image->map = q;
                            image->ismap = ismap;
                            image->touch = 0;
                            image->cache = getImage(image, base,
                                IMG_FLAG_SKIP);
                        } else if (iseq < 0) {
                            struct BufferPoint* po = buf->imarklist->marks - iseq - 1;
                            struct Anchor* a = retrieveAnchor(buf->img,
                                po->line, po->pos);
                            if (a) {
                                a_img->url = a->url;
                                a_img->image = a->image;
                            }
                        }
#endif
                    }
                    effect |= PE_IMAGE;
                    break;
                case HTML_N_IMG_ALT:
                    effect &= ~PE_IMAGE;
                    if (a_img) {
                        a_img->end.line = currentLn(buf);
                        a_img->end.pos = pos;
                    }
                    a_img = NULL;
                    break;
                case HTML_INPUT_ALT: {
                    struct FormList* form;
                    int top = 0, bottom = 0;
                    int textareanumber = -1;
#ifdef MENU_SELECT
                    int selectnumber = -1;
#endif
                    hseq = 0;
                    form_id = -1;

                    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
                    parsedtag_get_value(tag, ATTR_FID, &form_id);
                    parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
                    parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
                    if (form_id < 0 || form_id > hb->form_max || hb->forms == NULL || hb->forms[form_id] == NULL)
                        break; /* outside of <form>..</form> */
                    form = hb->forms[form_id];
                    if (hseq > 0) {
                        int hpos = pos;
                        if (*str == '[')
                            hpos++;
                        buf->hmarklist = putHmarker(buf->hmarklist, currentLn(buf),
                            hpos, hseq - 1);
                    } else if (hseq < 0) {
                        int h = -hseq - 1;
                        int hpos = pos;
                        if (*str == '[')
                            hpos++;
                        if (buf->hmarklist && h < buf->hmarklist->nmark && buf->hmarklist->marks[h].invalid) {
                            buf->hmarklist->marks[h].pos = hpos;
                            buf->hmarklist->marks[h].line = currentLn(buf);
                            buf->hmarklist->marks[h].invalid = 0;
                            hseq = -hseq;
                        }
                    }

                    if (!form->target)
                        form->target = buf->baseTarget;
                    if (a_textarea && parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &textareanumber)) {
                        if (textareanumber >= hb->max_textarea) {
                            hb->max_textarea = 2 * textareanumber;
                            hb->textarea_str = New_Reuse(Str, hb->textarea_str,
                                hb->max_textarea);
                            a_textarea = New_Reuse(struct Anchor*, a_textarea,
                                hb->max_textarea);
                        }
                    }

                    if (a_select && parsedtag_get_value(tag, ATTR_SELECTNUMBER, &selectnumber)) {
                        if (selectnumber >= hb->max_select) {
                            hb->max_select = 2 * selectnumber;
                            hb->select_option = New_Reuse(struct FormSelectOption,
                                hb->select_option,
                                hb->max_select);
                            a_select = New_Reuse(struct Anchor*, a_select,
                                hb->max_select);
                        }
                    }

                    a_form = registerForm(hb, buf, form, tag, currentLn(buf), pos);
                    if (a_textarea && textareanumber >= 0)
                        a_textarea[textareanumber] = a_form;
                    if (a_select && selectnumber >= 0)
                        a_select[selectnumber] = a_form;
                    if (a_form) {
                        a_form->hseq = hseq - 1;
                        a_form->y = currentLn(buf) - top;
                        a_form->rows = 1 + top + bottom;
                        if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
                            effect |= PE_FORM;
                        break;
                    }
                }
                case HTML_N_INPUT_ALT:
                    effect &= ~PE_FORM;
                    if (a_form) {
                        a_form->end.line = currentLn(buf);
                        a_form->end.pos = pos;
                        if (a_form->start.line == a_form->end.line && a_form->start.pos == a_form->end.pos)
                            a_form->hseq = -1;
                    }
                    a_form = NULL;
                    break;
                case HTML_MAP:
                    if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
                        struct MapList* m = New(struct MapList);
                        m->name = Strnew_charp(p);
                        m->area = newGeneralList();
                        m->next = buf->maplist;
                        buf->maplist = m;
                    }
                    break;
                case HTML_N_MAP:
                    /* nothing to do */
                    break;
                case HTML_AREA:
                    if (buf->maplist == NULL) /* outside of <map>..</map> */
                        break;
                    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
                        struct MapArea* a;
                        p = url_encode(remove_space(p), base,
                            buf->document_charset);
                        t = NULL;
                        parsedtag_get_value(tag, ATTR_TARGET, &t);
                        q = "";
                        parsedtag_get_value(tag, ATTR_ALT, &q);
                        r = NULL;
                        s = NULL;
#ifdef USE_IMAGE
                        parsedtag_get_value(tag, ATTR_SHAPE, &r);
                        parsedtag_get_value(tag, ATTR_COORDS, &s);
#endif
                        a = newMapArea(p, t, q, r, s);
                        pushValue(buf->maplist->area, (void*)a);
                    }
                    break;
                case HTML_FRAMESET:
                    frameset_sp++;
                    if (frameset_sp >= FRAMESTACK_SIZE)
                        break;
                    frameset_s[frameset_sp] = newFrameSet(tag);
                    if (frameset_s[frameset_sp] == NULL)
                        break;
                    if (frameset_sp == 0) {
                        if (buf->frameset == NULL) {
                            buf->frameset = frameset_s[frameset_sp];
                        } else
                            pushFrameTree(&(buf->frameQ),
                                frameset_s[frameset_sp], NULL);
                    } else
                        addFrameSetElement(frameset_s[frameset_sp - 1],
                            *(union frameset_element*)&frameset_s[frameset_sp]);
                    break;
                case HTML_N_FRAMESET:
                    if (frameset_sp >= 0)
                        frameset_sp--;
                    break;
                case HTML_FRAME:
                    if (frameset_sp >= 0 && frameset_sp < FRAMESTACK_SIZE) {
                        union frameset_element element;

                        element.body = newFrame(tag, buf);
                        addFrameSetElement(frameset_s[frameset_sp], element);
                    }
                    break;
                case HTML_BASE:
                    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
                        p = url_encode(remove_space(p), NULL,
                            buf->document_charset);
                        if (!buf->baseURL)
                            buf->baseURL = New(struct Url);
                        parseURL2(p, buf->baseURL, &buf->currentURL);
#if defined(USE_M17N) || defined(USE_IMAGE)
                        base = buf->baseURL;
#endif
                    }
                    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
                        buf->baseTarget = url_quote_conv(p, buf->document_charset);
                    break;
                case HTML_META:
                    p = q = NULL;
                    parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
                    parsedtag_get_value(tag, ATTR_CONTENT, &q);
                    if (p && q && !strcasecmp(p, "refresh") && getRuntime()->MetaRefresh) {
                        Str tmp = NULL;
                        int refresh_interval = getMetaRefreshParam(q, &tmp);
#ifdef USE_ALARM
                        if (tmp) {
                            p = url_encode(remove_space(tmp->ptr), base,
                                buf->document_charset);
                            buf->event = setAlarmEvent(buf->event,
                                refresh_interval,
                                AL_IMPLICIT_ONCE,
                                FUNCNAME_gorURL, (void*)p);
                        } else if (refresh_interval > 0)
                            buf->event = setAlarmEvent(buf->event,
                                refresh_interval,
                                AL_IMPLICIT,
                                FUNCNAME_reload, NULL);
#else
                        if (tmp && refresh_interval == 0) {
                            p = url_encode(remove_space(tmp->ptr), base,
                                buf->document_charset);
                            pushEvent(FUNCNAME_gorURL, p);
                        }
#endif
                    }
                    break;
                case HTML_INTERNAL:
                    internal = HTML_INTERNAL;
                    break;
                case HTML_N_INTERNAL:
                    internal = HTML_N_INTERNAL;
                    break;
                case HTML_FORM_INT:
                    if (parsedtag_get_value(tag, ATTR_FID, &form_id))
                        process_form_int(hb, tag, form_id);
                    break;
                case HTML_TEXTAREA_INT:
                    if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER,
                            &hb->n_textarea)
                        && hb->n_textarea >= 0 && hb->n_textarea < hb->max_textarea) {
                        hb->textarea_str[hb->n_textarea] = Strnew();
                    } else
                        hb->n_textarea = -1;
                    break;
                case HTML_N_TEXTAREA_INT:
                    if (a_textarea && hb->n_textarea >= 0) {
                        struct FormItemList* item = (struct FormItemList*)a_textarea[hb->n_textarea]->url;
                        item->init_value = item->value = hb->textarea_str[hb->n_textarea];
                    }
                    break;
#ifdef MENU_SELECT
                case HTML_SELECT_INT:
                    if (parsedtag_get_value(tag, ATTR_SELECTNUMBER, &hb->n_select)
                        && hb->n_select >= 0 && hb->n_select < hb->max_select) {
                        hb->select_option[hb->n_select].first = NULL;
                        hb->select_option[hb->n_select].last = NULL;
                    } else
                        hb->n_select = -1;
                    break;
                case HTML_N_SELECT_INT:
                    if (a_select && hb->n_select >= 0) {
                        struct FormItemList* item = (struct FormItemList*)a_select[hb->n_select]->url;
                        item->select_option = hb->select_option[hb->n_select].first;
                        chooseSelectOption(item, item->select_option);
                        item->init_selected = item->selected;
                        item->init_value = item->value;
                        item->init_label = item->label;
                    }
                    break;
                case HTML_OPTION_INT:
                    if (hb->n_select >= 0) {
                        int selected;
                        q = "";
                        parsedtag_get_value(tag, ATTR_LABEL, &q);
                        p = q;
                        parsedtag_get_value(tag, ATTR_VALUE, &p);
                        selected = parsedtag_exists(tag, ATTR_SELECTED);
                        addSelectOption(&hb->select_option[hb->n_select],
                            Strnew_charp(p), Strnew_charp(q),
                            selected);
                    }
                    break;
#endif
                case HTML_TITLE_ALT:
                    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
                        buf->buffername = html_unquote(p);
                    break;
                case HTML_SYMBOL:
                    effect |= PC_SYMBOL;
                    if (parsedtag_get_value(tag, ATTR_TYPE, &p))
                        symbol = (char)atoi(p);
                    break;
                case HTML_N_SYMBOL:
                    effect &= ~PC_SYMBOL;
                    break;
                }
#ifdef ID_EXT
                id = NULL;
                if (parsedtag_get_value(tag, ATTR_ID, &id)) {
                    id = url_quote_conv(id, name_charset);
                    registerName(buf, id, currentLn(buf), pos);
                }
                if (renderFrameSet && parsedtag_get_value(tag, ATTR_FRAMENAME, &p)) {
                    p = url_quote_conv(p, buf->document_charset);
                    if (!idFrame || strcmp(idFrame->body->name, p)) {
                        idFrame = search_frame(renderFrameSet, p);
                        if (idFrame && idFrame->body->attr != F_BODY)
                            idFrame = NULL;
                    }
                }
                if (id && idFrame)
                    idFrame->body->nameList = putAnchor(idFrame->body->nameList, id, NULL,
                        (struct Anchor**)NULL, NULL, NULL, '\0',
                        currentLn(buf), pos);
#endif /* ID_EXT */
            }
        }
        /* end of processing for one line */
        if (!internal)
            addnewline(&buf->doc, outc, outp, NULL, pos, -1, nlines);
        if (internal == HTML_N_INTERNAL)
            internal = 0;
        if (str != endp) {
            line = Strsubstr(line, str - line->ptr, endp - str);
            goto proc_again;
        }
    }
    for (form_id = 1; form_id <= hb->form_max; form_id++)
        if (hb->forms[form_id])
            hb->forms[form_id]->next = hb->forms[form_id - 1];
    buf->formlist = (hb->form_max >= 0) ? hb->forms[hb->form_max] : NULL;
    if (hb->n_textarea)
        addMultirowsForm(buf, buf->formitem);
#ifdef USE_IMAGE
    addMultirowsImg(buf, buf->img);
#endif
}

void HTMLlineproc2(struct HtmlBuilder* hb,
    struct Buffer* buf, TextLineList* tl)
{
    _tl_lp2 = tl->first;
    HTMLlineproc2body(hb, buf, textlist_feed, -1);
}

static struct input_stream* _file_lp2;

static Str
file_feed(void)
{
    Str s = is_get_str(_file_lp2, false);
    if (s && s->length == 0) {
        is_close(_file_lp2);
        return NULL;
    }
    return s;
}

static void
HTMLlineproc3(struct HtmlBuilder* hb,
    struct Buffer* buf, struct input_stream* stream)
{
    _file_lp2 = stream;
    HTMLlineproc2body(hb, buf, file_feed, -1);
}

static void
proc_escape(struct readbuffer* obuf, const char** str_return)
{
    const char *str = *str_return, *estr;
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
    Strcopy_charp_n(obuf->prevchar, estr, strlen(estr));
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

static int
table_width(struct HtmlBuilder* hb,
    struct html_feed_environ* h_env, int table_level)
{
    if (table_level < 0)
        return 0;
    int width = hb->tables[table_level]->total_width;
    if (table_level > 0 || width > 0)
        return width;
    return h_env->limit - h_env->envs[h_env->envc].indent;
}

/* HTML processing first pass */
void HTMLlineproc0(struct HtmlBuilder* hb,
    const char* line, struct html_feed_environ* h_env, bool internal)
{
    Lineprop mode;
    int cmd;
    struct readbuffer* obuf = h_env->obuf;
    int indent, delta;
    struct parsed_tag* tag;
    Str tokbuf;
    struct table* tbl = NULL;
    struct table_mode* tbl_mode = NULL;
    int tbl_width = 0;
    int is_hangul, prev_is_hangul = 0;

    tokbuf = Strnew();

table_start:
    if (obuf->table_level >= 0) {
        int level = min(obuf->table_level, MAX_TABLE - 1);
        tbl = hb->tables[level];
        tbl_mode = &hb->table_mode[level];
        tbl_width = table_width(hb, h_env, level);
    }

    while (*line != '\0') {
        const char *str, *p;
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
                str = Strnew_m_charp(getLinkNumberStr(hb, -1)->ptr, str, NULL)->ptr;
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
                feed_title(hb, str);
                continue;
            }
            /* select */
            if (pre_mode & RB_INSELECT) {
                if (obuf->table_level >= 0)
                    goto proc_normal;
                feed_select(hb, str);
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
                feed_textarea(hb, str);
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
            switch (feed_table(hb, tbl, str, tbl_mode, tbl_width, internal)) {
            case 0:
                /* </table> tag */
                obuf->table_level--;
                if (obuf->table_level >= MAX_TABLE - 1)
                    continue;
                end_table(tbl);
                if (obuf->table_level >= 0) {
                    struct table* tbl0 = hb->tables[obuf->table_level];
                    str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
                    if (tbl0->row < 0)
                        continue;
                    pushTable(tbl0, tbl);
                    tbl = tbl0;
                    tbl_mode = &hb->table_mode[obuf->table_level];
                    tbl_width = table_width(hb, h_env, obuf->table_level);
                    feed_table(hb, tbl, str, tbl_mode, tbl_width, TRUE);
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
                renderTable(hb, tbl, tbl_width, h_env);
                restore_fonteffect(h_env, obuf);
                obuf->flag &= ~RB_IGNORE_P;
                if (tbl->vspace > 0) {
                    int indent = h_env->envs[h_env->envc].indent;
                    do_blankline(h_env, obuf, indent, 0, h_env->limit);
                    obuf->flag |= RB_IGNORE_P;
                }
                Strcopy_charp_n(obuf->prevchar, " ", 1);
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
            if (HTMLtagproc1(hb, tag, h_env) == 0) {
                /* preserve the tag for second-stage processing */
                if (parsedtag_need_reconstruct(tag))
                    h_env->tagbuf = parsedtag2str(tag);
                push_tag(obuf, h_env->tagbuf->ptr, cmd);
            }
#ifdef ID_EXT
            else {
                process_idattr(obuf, cmd, tag);
            }
#endif /* ID_EXT */
            obuf->bp.init_flag = 1;
            clear_ignore_p_flag(cmd, obuf);
            if (cmd == HTML_TABLE)
                goto table_start;
            else {
                if (getRuntime()->displayLinkNumber && cmd == HTML_A && !internal)
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
                    const char* p = str;
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
                        PUSH(' ');
                    else
                        flushline(h_env, obuf, h_env->envs[h_env->envc].indent,
                            1, h_env->limit);
                } else if (ch == '\t') {
                    do {
                        PUSH(' ');
                    } while ((h_env->envs[h_env->envc].indent + obuf->pos)
                            % getRuntime()->Tabstop
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
                        PUSH(' ');
                    }
                    str++;
                } else {
#ifdef USE_M17N
                    if (mode == PC_KANJI1)
                        is_hangul = wtf_is_hangul((wc_uchar*)str);
                    else
                        is_hangul = 0;
                    if (!getRuntime()->SimplePreserveSpace && mode == PC_KANJI1 && !is_hangul && !prev_is_hangul && obuf->pos > h_env->envs[h_env->envc].indent && Strlastchar(obuf->line) == ' ') {
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
#endif
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
                    back_to_breakpoint(obuf);
                    flushline(h_env, obuf, indent, 0, h_env->limit);
                    HTMLlineproc0(hb, line->ptr, h_env, true);
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
            flushline(h_env, obuf, indent, 0, h_env->limit);
        }
    }
}

/*
 * loadHTMLBuffer: read file and make new buffer
 */
struct Buffer*
loadHTMLBuffer(struct Url url, struct input_stream* stream, const char* t,
    struct Buffer* newBuf, bool internal)
{
    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);

    FILE* src = NULL;
    Str tmp = NULL;
    if (newBuf->sourcefile == NULL && (url.scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(stream, newBuf, src, internal);

    if (src)
        fclose(src);

    return newBuf;
}

static char* _size_unit[] = { "b", "kb", "Mb", "Gb", "Tb",
    "Pb", "Eb", "Zb", "Bb", "Yb", NULL };

char* convert_size(int64_t size, int usefloat)
{
    float csize;
    int sizepos = 0;
    char** sizes = _size_unit;

    csize = (float)size;
    while (csize >= 999.495 && sizes[sizepos + 1]) {
        csize = csize / 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
        floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
        ->ptr;
}

char* convert_size2(int64_t size1, int64_t size2, int usefloat)
{
    char** sizes = _size_unit;
    float csize, factor = 1;
    int sizepos = 0;

    csize = (float)((size1 > size2) ? size1 : size2);
    while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
        factor *= 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
        floor(size1 / factor * 100.0 + 0.5) / 100.0,
        floor(size2 / factor * 100.0 + 0.5) / 100.0,
        sizes[sizepos])
        ->ptr;
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
    Strcopy_charp_n(obuf->prevchar, " ", 1);
    obuf->flag = RB_IGNORE_P;
    obuf->flag_sp = 0;
    obuf->status = R_ST_NORMAL;
    obuf->table_level = -1;
    obuf->nobr_level = 0;
    obuf->q_level = 0;
    bzero((void*)&obuf->anchor, sizeof(obuf->anchor));
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

void completeHTMLstream(struct HtmlBuilder* hb, struct html_feed_environ* h_env, struct readbuffer* obuf)
{
    close_anchor(hb, h_env, obuf);
    if (obuf->img_alt) {
        push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
        obuf->img_alt = NULL;
    }
    if (obuf->input_alt.in) {
        push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
        obuf->input_alt.hseq = 0;
        obuf->input_alt.fid = -1;
        obuf->input_alt.in = 0;
        obuf->input_alt.type = NULL;
        obuf->input_alt.name = NULL;
        obuf->input_alt.value = NULL;
    }
    if (obuf->in_bold) {
        push_tag(obuf, "</b>", HTML_N_B);
        obuf->in_bold = 0;
    }
    if (obuf->in_italic) {
        push_tag(obuf, "</i>", HTML_N_I);
        obuf->in_italic = 0;
    }
    if (obuf->in_under) {
        push_tag(obuf, "</u>", HTML_N_U);
        obuf->in_under = 0;
    }
    if (obuf->in_strike) {
        push_tag(obuf, "</s>", HTML_N_S);
        obuf->in_strike = 0;
    }
    if (obuf->in_ins) {
        push_tag(obuf, "</ins>", HTML_N_INS);
        obuf->in_ins = 0;
    }
    if (obuf->flag & RB_INTXTA)
        HTMLlineproc0(hb, "</textarea>", h_env, true);
    /* for unbalanced select tag */
    if (obuf->flag & RB_INSELECT)
        HTMLlineproc0(hb, "</select>", h_env, true);
    if (obuf->flag & RB_TITLE)
        HTMLlineproc0(hb, "</title>", h_env, true);

    /* for unbalanced table tag */
    if (obuf->table_level >= MAX_TABLE)
        obuf->table_level = MAX_TABLE - 1;

    while (obuf->table_level >= 0) {
        int tmp = obuf->table_level;
        hb->table_mode[obuf->table_level].pre_mode
            &= ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
        HTMLlineproc0(hb, "</table>", h_env, true);
        if (obuf->table_level >= tmp)
            break;
    }
}

static void
print_internal_information(struct HtmlBuilder* hb, struct html_feed_environ* henv)
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

    if (hb->n_select > 0) {
        struct FormSelectOptionItem* ip;
        for (i = 0; i < hb->n_select; i++) {
            s = Sprintf("<select_int selectnumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            for (ip = hb->select_option[i].first; ip; ip = ip->next) {
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

    if (hb->n_textarea > 0) {
        for (i = 0; i < hb->n_textarea; i++) {
            s = Sprintf("<textarea_int textareanumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            s = Strnew_charp(html_quote(hb->textarea_str[i]->ptr));
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

void loadHTMLstream(struct input_stream* stream,
    struct Buffer* newBuf, FILE* src, bool internal)
{
    struct HtmlBuilder _hb = {
        0,
        .max_textarea = MAX_TEXTAREA,
        .textarea_str = New_N(Str, MAX_TEXTAREA),
    };
    struct HtmlBuilder* hb = &_hb;

    struct environment envs[MAX_ENV_LEVEL];
    int64_t linelen = 0;
    int64_t trbyte = 0;
    Str lineBuf2 = Strnew();
    wc_ces charset = WC_CES_US_ASCII;
    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;
    int volatile image_flag;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    if (fmInitialized() && graph_ok()) {
        symbol_width = symbol_width0 = 1;
    } else {
        symbol_width0 = 0;
        get_symbol(getRuntime()->DisplayCharset, &symbol_width0);
        symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    }

    hb->n_select = 0;
    hb->max_select = MAX_SELECT;
    hb->select_option = New_N(struct FormSelectOption, hb->max_select);

    hb->form_sp = -1;
    hb->form_max = -1;
    hb->forms_size = 0;
    hb->forms = NULL;
    hb->cur_hseq = 1;
    hb->cur_iseq = 1;
    if (newBuf->image_flag)
        image_flag = newBuf->image_flag;
    else if (getRuntime()->activeImage && getRuntime()->displayImage && getRuntime()->autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;

    if (getRuntime()->w3m_halfload) {
        newBuf->buffername = "---";

        newBuf->document_charset = getRuntime()->InnerCharset;

        HTMLlineproc3(hb, newBuf, stream);
        getRuntime()->w3m_halfload = FALSE;
        return;
    }

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);

    if (w3m_halfdump)
        htmlenv1.f = stdout;
    else
        htmlenv1.buf = newTextLineList();
    hb->cur_baseURL = baseURL(newBuf);

    if (SETJMP(AbortLoading) != 0) {
        HTMLlineproc0(hb, "<br>Transfer Interrupted!<br>", &htmlenv1, true);
        goto phase2;
    }
    TRAP_ON;

    wc_ces doc_charset = getRuntime()->DocumentCharset;
    if (newBuf) {
        if (newBuf->bufferprop & BP_FRAME)
            charset = getRuntime()->InnerCharset;
        else if (newBuf->document_charset)
            charset = doc_charset = newBuf->document_charset;
    }
    if (newBuf->content.content_charset && getRuntime()->UseContentCharset)
        doc_charset = newBuf->content.content_charset;
    hb->meta_charset = 0;

    while ((lineBuf2 = is_get_str(stream, true)) && lineBuf2->length) {

        if (src)
            Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        if (getRuntime()->w3m_dump & DUMP_EXTRA)
            printf("W3m-in-progress: %s\n", convert_size2(linelen, newBuf->content.current_content_length, TRUE));
        if (getRuntime()->w3m_dump & DUMP_SOURCE)
            continue;
        showProgress(&linelen, &trbyte, newBuf->content.current_content_length);
        /*
         * if (frame_source)
         * continue;
         */

        if (hb->meta_charset) { /* <META> */
            if (newBuf->content.content_charset == 0 && getRuntime()->UseContentCharset) {
                doc_charset = hb->meta_charset;
                charset = WC_CES_US_ASCII;
            }
            hb->meta_charset = 0;
        }

        lineBuf2 = convertLine(lineBuf2, HTML_MODE, &charset, doc_charset);

        hb->cur_document_charset = charset;

        HTMLlineproc0(hb, lineBuf2->ptr, &htmlenv1, internal);
    }
    if (obuf.status != R_ST_NORMAL) {
        HTMLlineproc0(hb, "\n", &htmlenv1, internal);
    }
    obuf.status = R_ST_NORMAL;
    completeHTMLstream(hb, &htmlenv1, &obuf);
    flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);

    if (htmlenv1.title)
        newBuf->buffername = htmlenv1.title;
    if (w3m_halfdump) {
        TRAP_OFF;
        print_internal_information(hb, &htmlenv1);
        return;
    }
    if (getRuntime()->w3m_backend) {
        TRAP_OFF;
        print_internal_information(hb, &htmlenv1);
        backend_halfdump_buf = htmlenv1.buf;
        return;
    }

phase2:
    newBuf->trbyte = trbyte + linelen;
    TRAP_OFF;
    if (!(newBuf->bufferprop & BP_FRAME))
        newBuf->document_charset = charset;
    newBuf->image_flag = image_flag;
    HTMLlineproc2(hb, newBuf, htmlenv1.buf);

    newBuf->doc.topLine = newBuf->doc.firstLine;
    newBuf->doc.lastLine = newBuf->doc.currentLine;
    newBuf->doc.currentLine = newBuf->doc.firstLine;
    newBuf->type = "text/html";
    if (hb->n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
}

/*
 * loadHTMLString: read string and make new buffer
 */
struct Buffer*
loadHTMLString(Str page)
{
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    struct input_stream* stream = is_from_str(page);
    struct Buffer* newBuf = newBuffer(INIT_BUFFER_WIDTH);
    if (SETJMP(AbortLoading) != 0) {
        TRAP_OFF;
        discardBuffer(newBuf);
        is_close(stream);
        return NULL;
    }
    TRAP_ON;

    newBuf->document_charset = getRuntime()->InnerCharset;
    loadHTMLstream(stream, newBuf, NULL, TRUE);
    newBuf->document_charset = WC_CES_US_ASCII;

    TRAP_OFF;
    is_close(stream);
    return newBuf;
}

/*
 * loadBuffer: read file and make new buffer
 */
struct Buffer*
loadBuffer(struct Url url, struct input_stream* stream,
    const char* t, struct Buffer* volatile newBuf, bool internal)
{
    FILE* volatile src = NULL;

    wc_ces charset = WC_CES_US_ASCII;
    wc_ces volatile doc_charset = getRuntime()->DocumentCharset;

    Str lineBuf2;
    volatile char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    int64_t linelen = 0, trbyte = 0;
    Lineprop* propBuffer = NULL;
    Linecolor* colorBuffer = NULL;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);

    if (SETJMP(AbortLoading) != 0) {
        goto _end;
    }
    TRAP_ON;

    if (newBuf->sourcefile == NULL && (url.scheme != SCM_LOCAL || newBuf->mailcap)) {
        tmpf = tmpfname(TMPF_SRC, NULL);
        src = fopen(tmpf->ptr, "w");
        if (src)
            newBuf->sourcefile = tmpf->ptr;
    }
    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (newBuf->content.content_charset && getRuntime()->UseContentCharset)
        doc_charset = newBuf->content.content_charset;

    nlines = 0;
    while ((lineBuf2 = is_get_str(stream, true)) && lineBuf2->length) {
        if (src)
            Strfputs(lineBuf2, src);
        linelen += lineBuf2->length;
        if (getRuntime()->w3m_dump & DUMP_EXTRA)
            printf("W3m-in-progress: %s\n", convert_size2(linelen, newBuf->content.current_content_length, TRUE));
        if (getRuntime()->w3m_dump & DUMP_SOURCE)
            continue;
        showProgress(&linelen, &trbyte, newBuf->content.current_content_length);
        if (frame_source)
            continue;
        lineBuf2 = convertLine(lineBuf2, PAGER_MODE, &charset, doc_charset);
        if (getRuntime()->squeezeBlankLine) {
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
                ++nlines;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        Strchop(lineBuf2);
        lineBuf2 = checkType(lineBuf2, &propBuffer, NULL);
        addnewline(&newBuf->doc, lineBuf2->ptr, propBuffer, colorBuffer,
            lineBuf2->length, FOLD_BUFFER_WIDTH, nlines);
    }
_end:
    TRAP_OFF;
    newBuf->doc.topLine = newBuf->doc.firstLine;
    newBuf->doc.lastLine = newBuf->doc.currentLine;
    newBuf->doc.currentLine = newBuf->doc.firstLine;
    newBuf->trbyte = trbyte + linelen;
#ifdef USE_M17N
    newBuf->document_charset = charset;
#endif
    if (src)
        fclose(src);

    return newBuf;
}

struct Buffer*
loadImageBuffer(struct Url url, struct input_stream* stream,
    const char* t, struct Buffer* newBuf, bool internal)
{
    struct Image image;
    struct ImageCache* cache;
    Str tmp, tmpf;
    FILE* src = NULL;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;
    struct stat st;
    const struct Url* pu = newBuf ? &newBuf->currentURL : NULL;

    loadImage(newBuf, IMG_FLAG_STOP);
    image.url = parsedURL2Str(&url)->ptr;
    image.ext = filename_extension(url.file, true);
    image.width = -1;
    image.height = -1;
    image.cache = NULL;
    cache = getImage(&image, (struct Url*)pu, IMG_FLAG_AUTO);
    if (!(pu && pu->is_nocache) && cache->loaded & IMG_FLAG_LOADED && !stat(cache->file, &st))
        goto image_buffer;

    TRAP_ON;
    if (!is_save2tmp(stream, cache->file)) {
        TRAP_OFF;
        return NULL;
    }
    TRAP_OFF;

    cache->loaded = IMG_FLAG_LOADED;
    cache->index = 0;

image_buffer:
    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);
    cache->loaded |= IMG_FLAG_DONT_REMOVE;
    if (newBuf->sourcefile == NULL && url.scheme != SCM_LOCAL)
        newBuf->sourcefile = cache->file;

    tmp = Sprintf("<img src=\"%s\"><br><br>", html_quote(image.url));
    tmpf = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmpf->ptr, "w");
    if (src == NULL)
        return NULL;
    newBuf->mailcap_source = tmpf->ptr;

    struct input_stream* tmp_stream = is_from_str(tmp);
    loadHTMLstream(tmp_stream, newBuf, src, true);
    is_close(tmp_stream);
    if (src)
        fclose(src);

    newBuf->doc.topLine = newBuf->doc.firstLine;
    newBuf->doc.lastLine = newBuf->doc.currentLine;
    newBuf->doc.currentLine = newBuf->doc.firstLine;
    newBuf->image_flag = IMG_FLAG_AUTO;
    return newBuf;
}

static Str
conv_symbol(struct Line* l)
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
                symbol = get_symbol(getRuntime()->DisplayCharset, &w);
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
_saveBuffer(struct Buffer* buf, struct Line* l, FILE* f, int cont)
{
    Str tmp;
    int is_html = FALSE;

    // int set_charset = !getRuntime()->DisplayCharset;
    wc_ces charset = getRuntime()->DisplayCharset
        ? getRuntime()->DisplayCharset
        : WC_CES_US_ASCII;
    is_html = is_html_type(buf->type);

    // pager_next:
    for (; l != NULL; l = l->next) {
        if (is_html)
            tmp = conv_symbol(l);
        else
            tmp = Strnew_charp_n(l->lineBuf, l->len);
        tmp = wc_Str_conv(tmp, getRuntime()->InnerCharset, charset);
        Strfputs(tmp, f);
        if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
            putc('\n', f);
    }
    // if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
    //     l = getNextPage(buf, PagerMax);
    //
    //     if (set_charset)
    //         charset = buf->document_charset;
    //
    //     goto pager_next;
    // }
}

void saveBuffer(struct Buffer* buf, FILE* f, int cont)
{
    _saveBuffer(buf, buf->doc.firstLine, f, cont);
}

void saveBufferBody(struct Buffer* buf, FILE* f, int cont)
{
    struct Line* l = buf->doc.firstLine;

    while (l != NULL && l->real_linenumber == 0)
        l = l->next;
    _saveBuffer(buf, l, f, cont);
}

static struct Buffer*
loadcmdout(char* cmd,
    LoadBufferFunc loadproc, struct Buffer* defaultbuf)
{
    if (cmd == NULL || *cmd == '\0')
        return NULL;

    FILE* f = popen(cmd, "r");
    if (f == NULL)
        return NULL;

    struct input_stream* stream = is_from_file(f, pclose);
    struct Url url;
    parseURL(cmd, &url, NULL);
    struct Buffer* buf = loadproc(url, stream, NULL,
        defaultbuf, defaultbuf->bufferprop & BP_FRAME);
    is_close(stream);
    return buf;
}

/*
 * getshell: execute shell command and get the result into a buffer
 */
struct Buffer*
getshell(char* cmd)
{
    struct Buffer* buf;

    buf = loadcmdout(cmd, loadBuffer, NULL);
    if (buf == NULL)
        return NULL;
    buf->content.filename = cmd;
    buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME,
        conv_from_system(cmd))
                          ->ptr;
    return buf;
}

struct Buffer*
doExternal(struct Url url, struct input_stream* stream,
    const char* type, struct Buffer* defaultbuf, bool internal)
{
    Str command;
    struct mailcap* mcap;
    int mc_stat;
    struct Buffer* buf = NULL;
    const char* src = NULL;
    const char* ext = filename_extension(url.file, true);

    if (!(mcap = searchExtViewer(type)))
        return NULL;

    if (mcap->nametemplate) {
        Str _tmp = unquote_mailcap(mcap->nametemplate, NULL, "", NULL, NULL);
        if (_tmp->ptr[0] == '.')
            ext = _tmp->ptr;
    }

    Str tmpf = tmpfname(TMPF_DFL, (ext && *ext) ? ext : NULL);
    const char* header = checkHeader(&defaultbuf->content, "Content-Type:");
    if (header)
        header = conv_to_system(header);
    command = unquote_mailcap(mcap->viewer, type, tmpf->ptr, header, &mc_stat);
    if (!(mc_stat & MCSTAT_REPNAME)) {
        Str tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
        command = tmp;
    }

    if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) && !(mcap->flags & MAILCAP_NEEDSTERMINAL) && getRuntime()->BackgroundExtViewer) {
        flush_tty();
        if (!fork()) {
            setup_child(FALSE, 0, is_file_no(stream));
            if (!is_save2tmp(stream, tmpf->ptr))
                exit(1);
            is_close(stream);
            myExec(command->ptr);
        }
        return NO_BUFFER;
    } else {
        if (!is_save2tmp(stream, tmpf->ptr)) {
            return NULL;
        }
    }
    if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
        if (defaultbuf == NULL)
            defaultbuf = newBuffer(INIT_BUFFER_WIDTH);
        if (defaultbuf->sourcefile)
            src = defaultbuf->sourcefile;
        else
            src = tmpf->ptr;
        defaultbuf->sourcefile = NULL;
        defaultbuf->mailcap = mcap;
    }
    if (mcap->flags & MAILCAP_HTMLOUTPUT) {
        buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
        if (buf && buf != NO_BUFFER) {
            buf->type = "text/html";
            buf->mailcap_source = buf->sourcefile;
            buf->sourcefile = src;
        }
    } else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
        buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
        if (buf && buf != NO_BUFFER) {
            buf->type = "text/plain";
            buf->mailcap_source = buf->sourcefile;
            buf->sourcefile = src;
        }
    } else {
        if (mcap->flags & MAILCAP_NEEDSTERMINAL || !getRuntime()->BackgroundExtViewer) {
            exitRawMode();
            mySystem(command->ptr, 0);
            enterRawMode();
        } else {
            mySystem(command->ptr, 1);
        }
        buf = NO_BUFFER;
    }
    if (buf && buf != NO_BUFFER) {
        if ((buf->buffername == NULL || buf->buffername[0] == '\0') && buf->content.filename)
            buf->buffername = conv_from_system(lastFileName(buf->content.filename));
        buf->edit = mcap->edit;
        buf->mailcap = mcap;
    }
    return buf;
}

static int
_MoveFile(const char* path1, const char* path2)
{
    struct input_stream* f1 = is_from_fd(open(path1, O_RDONLY));
    if (!f1)
        return -1;

    FILE* f2;
    int is_pipe;
    int64_t linelen = 0, trbyte = 0;
    char* buf = NULL;
    int count;

    if (*path2 == '|' && getRuntime()->PermitSaveToPipe) {
        is_pipe = TRUE;
        f2 = popen(path2 + 1, "w");
    } else {
        is_pipe = FALSE;
        f2 = fopen(path2, "wb");
    }
    if (f2 == NULL) {
        is_close(f1);
        return -1;
    }
    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = is_read(f1, buf, SAVE_BUF_SIZE)) > 0) {
        fwrite(buf, 1, count, f2);
        linelen += count;
        showProgress(&linelen, &trbyte, 0);
    }
    xfree(buf);
    is_close(f1);
    if (is_pipe)
        pclose(f2);
    else
        fclose(f2);
    return 0;
}

int _doFileCopy(const char* tmpf, const char* defstr, bool download)
{
#ifndef __MINGW32_VERSION
    Str msg;
    Str filen;
    char *p, *q = NULL;
    pid_t pid;
    char* lock;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
    FILE* f;
#endif
    struct stat st;
    int64_t size = 0;
    int is_pipe = FALSE;

    if (fmInitialized()) {
        p = searchKeyData();
        if (p == NULL || *p == '\0') {
            /* FIXME: gettextize? */
            q = inputLineHist("(Download)Save file to: ",
                defstr, IN_COMMAND, getRuntime()->SaveHist);
            if (q == NULL || *q == '\0')
                return FALSE;
            p = conv_to_system(q);
        }
        if (*p == '|' && getRuntime()->PermitSaveToPipe)
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
            disp_err_message(msg->ptr, FALSE);
            return -1;
        }
        if (!download) {
            if (_MoveFile(tmpf, p) < 0) {
                /* FIXME: gettextize? */
                msg = Sprintf("Can't save to %s", conv_from_system(p));
                disp_err_message(msg->ptr, FALSE);
            }
            return -1;
        }
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;
        symlink(p, lock);
        flush_tty();
        pid = fork();
        if (!pid) {
            setup_child(FALSE, 0, -1);
            if (!_MoveFile(tmpf, p) && getRuntime()->PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
                setModtime(p, st.st_mtime);
            unlink(lock);
            exit(0);
        }
        if (!stat(tmpf, &st))
            size = st.st_size;
        addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
    } else {
        q = searchKeyData();
        if (q == NULL || *q == '\0') {
            /* FIXME: gettextize? */
            printf("(Download)Save file to: ");
            fflush(stdout);
            filen = Strfgets(stdin);
            if (filen->length == 0)
                return -1;
            q = filen->ptr;
        }
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        *(p + 1) = '\0';
        if (*q == '\0')
            return -1;
        p = q;
        if (*p == '|' && getRuntime()->PermitSaveToPipe)
            is_pipe = TRUE;
        else {
            p = expandPath(p);
            if (checkOverWrite(p) < 0)
                return -1;
        }
        if (checkCopyFile(tmpf, p) < 0) {
            /* FIXME: gettextize? */
            printf("Can't copy. %s and %s are identical.", tmpf, p);
            return -1;
        }
        if (_MoveFile(tmpf, p) < 0) {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return -1;
        }
        if (getRuntime()->PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
            setModtime(p, st.st_mtime);
    }
#endif /* __MINGW32_VERSION */
    return 0;
}

int doFileMove(const char* tmpf, const char* defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
}

int doFileSave(struct Url url, struct input_stream* stream,
    const char* defstr, enum CompressionType compression)
{
    if (fmInitialized()) {
        const char* p = searchKeyData();
        if (p == NULL || *p == '\0') {
            p = inputLineHist("(Download)Save file to: ",
                defstr, IN_FILENAME, getRuntime()->SaveHist);
            if (p == NULL || *p == '\0')
                return -1;
            p = conv_to_system(p);
        }
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(stream, p) < 0) {
            Str msg = Sprintf("Can't save. Load file and %s are identical.",
                conv_from_system(p));
            disp_err_message(msg->ptr, FALSE);
            return -1;
        }

        const char* lock = tmpfname(TMPF_DFL, ".lock")->ptr;
        symlink(p, lock);
        flush_tty();
        pid_t pid = fork();
        if (!pid) {
            if ((compression != CMP_NOCOMPRESS) && getRuntime()->AutoUncompress) {
                stream = uncompress_stream(stream, compression, NULL);
            }

            setup_child(FALSE, 0, is_file_no(stream));
            bool succeeded = is_save2tmp(stream, p);
            // if (succeeded && getRuntime()->PreserveTimestamp && uf.modtime != -1)
            //     setModtime(p, uf.modtime);
            is_close(stream);
            unlink(lock);
            if (!succeeded)
                exit(1);
            exit(0);
        }
        addDownloadList(
            pid, parsedURL2RefererStr(&url)->ptr, p, lock, 0);
    } else {
        char* q = searchKeyData();
        if (q == NULL || *q == '\0') {
            printf("(Download)Save file to: ");
            fflush(stdout);
            Str filen = Strfgets(stdin);
            if (filen->length == 0)
                return -1;
            q = filen->ptr;
        }
        char* p;
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        *(p + 1) = '\0';
        if (*q == '\0')
            return -1;
        p = expandPath(q);
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(stream, p) < 0) {
            printf("Can't save. Load file and %s are identical.", p);
            return -1;
        }
        if (compression != CMP_NOCOMPRESS && getRuntime()->AutoUncompress) {
            stream = uncompress_stream(stream, compression, NULL);
        }
        if (!is_save2tmp(stream, p)) {
            printf("Can't save to %s\n", p);
            return -1;
        }
        // if (getRuntime()->PreserveTimestamp && uf.modtime != -1)
        //     setModtime(p, uf.modtime);
    }
    return 0;
}

int checkCopyFile(const char* path1, const char* path2)
{
    struct stat st1, st2;

    if (*path2 == '|' && getRuntime()->PermitSaveToPipe)
        return 0;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkSaveFile(struct input_stream* stream, const char* path2)
{
    struct stat st1, st2;
    int des = is_file_no(stream);

    if (des < 0)
        return 0;
    if (*path2 == '|' && getRuntime()->PermitSaveToPipe)
        return 0;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkOverWrite(const char* path)
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
