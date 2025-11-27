#include "http_auth.h"
#include "tui.h"
#include "form.h"
#include "indep.h"
#include <openssl/md5.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define PASSWD_FILE RC_DIR "/passwd"

int disable_secret_security_check = (false);
const char* passwd_file = (PASSWD_FILE);

enum {
    QOP_NONE,
    QOP_AUTH,
    QOP_AUTH_INT,
};

static struct auth_param none_auth_param[] = {
    { NULL, NULL }
};

static struct auth_param basic_auth_param[] = {
    { "realm", NULL },
    { NULL, NULL }
};

struct auth_pass {
    bool bad;
    int is_proxy;
    Str host;
    int port;
    /*    Str file; */
    Str realm;
    Str uname;
    Str pwd;
    struct auth_pass* next;
};

struct auth_pass* passwords = NULL;

/*
 * RFC2617: 1.2 Access Authentication Framework
 *
 * The realm value (case-sensitive), in combination with the canonical root
 * URL (the absoluteURI for the server whose abs_path is empty; see section
 * 5.1.2 of RFC2616 ) of the server being accessed, defines the protection
 * space. These realms allow the protected resources on a server to be
 * partitioned into a set of protection spaces, each with its own
 * authentication scheme and/or authorization database.
 *
 */
static void
add_auth_pass_entry(const struct auth_pass* ent, int netrc, int override)
{
    if ((ent->host || netrc) /* netrc accept default (host == NULL) */
        && (ent->is_proxy || ent->realm || netrc)
        && ent->uname && ent->pwd) {
        struct auth_pass* newent = New(struct auth_pass);
        memcpy(newent, ent, sizeof(struct auth_pass));
        if (override) {
            newent->next = passwords;
            passwords = newent;
        } else {
            if (passwords == NULL)
                passwords = newent;
            else if (passwords->next == NULL)
                passwords->next = newent;
            else {
                struct auth_pass* ep = passwords;
                for (; ep->next; ep = ep->next)
                    ;
                ep->next = newent;
            }
        }
    }
    /* ignore invalid entries */
}

void add_auth_user_passwd(struct Url* pu, char* realm, Str uname, Str pwd,
    int is_proxy)
{
    struct auth_pass ent;
    memset(&ent, 0, sizeof(ent));

    ent.is_proxy = is_proxy;
    ent.host = Strnew_charp(pu->host);
    ent.port = pu->port;
    ent.realm = Strnew_charp(realm);
    ent.uname = uname;
    ent.pwd = pwd;
    add_auth_pass_entry(&ent, 0, 1);
}

static struct auth_pass*
find_auth_pass_entry(char* host, int port, char* realm, char* uname,
    int is_proxy)
{
    struct auth_pass* ent;
    for (ent = passwords; ent != NULL; ent = ent->next) {
        if (ent->is_proxy == is_proxy
            && (!ent->bad)
            && (!ent->host || !Strcasecmp_charp(ent->host, host))
            && (!ent->port || ent->port == port)
            && (!ent->uname || !uname || !Strcmp_charp(ent->uname, uname))
            && (!ent->realm || !realm || !Strcmp_charp(ent->realm, realm)))
            return ent;
    }
    return NULL;
}

int find_auth_user_passwd(struct Url* pu, char* realm, Str* uname, Str* pwd, int is_proxy)
{
    struct auth_pass* ent;

    if (pu->user && pu->pass) {
        *uname = Strnew_charp(pu->user);
        *pwd = Strnew_charp(pu->pass);
        return 1;
    }
    ent = find_auth_pass_entry(pu->host, pu->port, realm, pu->user, is_proxy);
    if (ent) {
        *uname = ent->uname;
        *pwd = ent->pwd;
        return 1;
    }
    return 0;
}

/* passwd */
/*
 * machine <host>
 * host <host>
 * port <port>
 * proxy
 * path <file>	; not used
 * realm <realm>
 * login <login>
 * passwd <passwd>
 * password <passwd>
 */

static Str
next_token(Str arg)
{
    Str narg = NULL;
    char *p, *q;
    if (arg == NULL || arg->length == 0)
        return NULL;
    p = arg->ptr;
    q = p;
    SKIP_NON_BLANKS(&q);
    if (*q != '\0') {
        *q++ = '\0';
        SKIP_BLANKS(&q);
        if (*q != '\0')
            narg = Strnew_charp(q);
    }
    return narg;
}
static void
parsePasswd(FILE* fp, int netrc)
{
    struct auth_pass ent;
    Str line = NULL;

    bzero(&ent, sizeof(struct auth_pass));
    while (1) {
        Str arg = NULL;
        char* p;

        if (line == NULL || line->length == 0)
            line = Strfgets(fp);
        if (line->length == 0)
            break;
        Strchop(line);
        Strremovefirstspaces(line);
        p = line->ptr;
        if (*p == '#' || *p == '\0') {
            line = NULL;
            continue; /* comment or empty line */
        }
        arg = next_token(line);

        if (!strcmp(p, "machine") || !strcmp(p, "host")
            || (netrc && !strcmp(p, "default"))) {
            add_auth_pass_entry(&ent, netrc, 0);
            bzero(&ent, sizeof(struct auth_pass));
            if (netrc)
                ent.port = 21; /* XXX: getservbyname("ftp"); ? */
            if (strcmp(p, "default") != 0) {
                line = next_token(arg);
                ent.host = arg;
            } else {
                line = arg;
            }
        } else if (!netrc && !strcmp(p, "port") && arg) {
            line = next_token(arg);
            ent.port = atoi(arg->ptr);
        } else if (!netrc && !strcmp(p, "proxy")) {
            ent.is_proxy = 1;
            line = arg;
        } else if (!netrc && !strcmp(p, "path")) {
            line = next_token(arg);
            /* ent.file = arg; */
        } else if (!netrc && !strcmp(p, "realm")) {
            /* XXX: rest of line becomes arg for realm */
            line = NULL;
            ent.realm = arg;
        } else if (!strcmp(p, "login")) {
            line = next_token(arg);
            ent.uname = arg;
        } else if (!strcmp(p, "password") || !strcmp(p, "passwd")) {
            line = next_token(arg);
            ent.pwd = arg;
        } else if (netrc && !strcmp(p, "machdef")) {
            while ((line = Strfgets(fp))->length != 0) {
                if (*line->ptr == '\n')
                    break;
            }
            line = NULL;
        } else if (netrc && !strcmp(p, "account")) {
            /* ignore */
            line = next_token(arg);
        } else {
            /* ignore rest of line */
            line = NULL;
        }
    }
    add_auth_pass_entry(&ent, netrc, 0);
}

/* FIXME: gettextize? */
#define FILE_IS_READABLE_MSG "SECURITY NOTE: file %s must not be accessible by others"

FILE* openSecretFile(const char* fname)
{
    if (fname == NULL)
        return NULL;

    const char* efname = expandPath(fname);
    struct stat st;
    if (stat(efname, &st) < 0)
        return NULL;

    /* check permissions, if group or others readable or writable,
     * refuse it, because it's insecure.
     *
     * XXX: disable_secret_security_check will introduce some
     *    security issues, but on some platform such as Windows
     *    it's not possible (or feasible) to disable group|other
     *    readable and writable.
     *   [w3m-dev 03368][w3m-dev 03369][w3m-dev 03370]
     */
    if (disable_secret_security_check)
        /* do nothing */;
    else if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        // if (fmInitialized) {
        //     message(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, 0, 0);
        //     refresh();
        // } else
        {
            fputs(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, stderr);
            fputc('\n', stderr);
        }
        sleep(2);
        return NULL;
    }

    return fopen(efname, "r");
}

void loadPasswd(void)
{
    passwords = NULL;
    FILE* fp = openSecretFile(passwd_file);
    if (fp != NULL) {
        parsePasswd(fp, 0);
        fclose(fp);
    }

    /* for FTP */
    fp = openSecretFile("~/.netrc");
    if (fp != NULL) {
        parsePasswd(fp, 1);
        fclose(fp);
    }
    return;
}

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
static struct auth_param digest_auth_param[] = {
    { "realm", NULL },
    { "domain", NULL },
    { "nonce", NULL },
    { "opaque", NULL },
    { "stale", NULL },
    { "algorithm", NULL },
    { "qop", NULL },
    { NULL, NULL }
};

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

static Str
AuthBasicCred(struct http_auth* ha, Str uname, Str pw, struct Url* pu,
    struct HttpRequest* hr, FormList* request)
{
    Str s = Strdup(uname);
    Strcat_char(s, ':');
    Strcat(s, pw);
    return Strnew_m_charp("Basic ", base64_encode(s->ptr, s->length)->ptr, NULL);
}

static Str
AuthDigestCred(struct http_auth* ha, Str uname, Str pw, struct Url* pu,
    struct HttpRequest* hr, FormList* request)
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
        char* p;
        size_t i;

        p = qop->ptr;
        SKIP_BLANKS(&p);

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
                SKIP_BLANKS(&p);
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

/* for RFC2617: HTTP Authentication */
static struct http_auth www_auth[] = {
    { 1, "Basic ", basic_auth_param, AuthBasicCred },
    { 10, "Digest ", digest_auth_param, AuthDigestCred },
    {
        0,
        NULL,
        NULL,
        NULL,
    }
};

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
    bool quoted = false;
    Str val = Strnew();

    SKIP_BLANKS((char**)&qq);
    if (*qq == '"') {
        quoted = true;
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
        SKIP_BLANKS(&q);
        for (ap = auth; ap->name != NULL; ap++) {
            size_t len;

            len = strlen(ap->name);
            if (strncasecmp(q, ap->name, len) == 0 && (IS_SPACE(q[len]) || q[len] == '=')) {
                p = q + len;
                SKIP_BLANKS(&p);
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
                SKIP_BLANKS(&q);
                if (*q != '=')
                    return p;
                q++;
                extract_auth_val(&q);
            } else
                return p;
        }
        if (*q != '\0') {
            SKIP_BLANKS(&q);
            if (*q == ',')
                q++;
            else
                break;
        }
    }
    return q;
}

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

void invalidate_auth_user_passwd(struct Url* pu, char* realm, Str uname, Str pwd,
    int is_proxy)
{
    struct auth_pass* ent = find_auth_pass_entry(pu->host, pu->port, realm, NULL, is_proxy);
    if (ent) {
        ent->bad = true;
    }
    return;
}

void getAuthCookie(struct http_auth* hauth, char* auth_header,
    TextList* extra_header, struct Url* pu, struct HttpRequest* hr,
    FormList* request,
    volatile Str* uname, volatile Str* pwd)
{
    Str ss = NULL;
    Str tmp;
    TextListItem* i;
    int auth_header_len = strlen(auth_header);
    char* realm = NULL;
    int proxy;

    if (hauth)
        realm = qstr_unquote(get_auth_param(hauth->param, "realm"))->ptr;

    if (!realm)
        return;

    bool a_found = false;
    for (i = extra_header->first; i != NULL; i = i->next) {
        if (!strncasecmp(i->ptr, auth_header, auth_header_len)) {
            a_found = true;
            break;
        }
    }
    proxy = !strncasecmp("Proxy-Authorization:", auth_header,
        auth_header_len);
    if (a_found) {
        /* This means that *-Authenticate: header is received after
         * Authorization: header is sent to the server.
         */
        // if (fmInitialized) {
        //     message("Wrong username or password", 0, 0);
        //     refresh();
        // } else
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
        tui_input_pw(realm, uname, pwd);
    }
    ss = hauth->cred(hauth, *uname, *pwd, pu, hr, request);
    if (ss) {
        tmp = Strnew_charp(auth_header);
        Strcat_m_charp(tmp, " ", ss->ptr, "\r\n", NULL);
        pushText(extra_header, tmp->ptr);
    } else {
        *uname = NULL;
        *pwd = NULL;
    }
    return;
}

Str get_auth_param(struct auth_param* auth, char* name)
{
    struct auth_param* ap;
    for (ap = auth; ap->name != NULL; ap++) {
        if (strcasecmp(name, ap->name) == 0)
            return ap->val;
    }
    return NULL;
}

struct http_auth*
findAuthentication(struct http_auth* hauth, TextList* document_header, char* auth_field)
{
    struct http_auth* ha;
    int len = strlen(auth_field), slen;
    TextListItem* i;
    char *p0, *p;

    bzero(hauth, sizeof(struct http_auth));
    for (i = document_header->first; i != NULL; i = i->next) {
        if (strncasecmp(i->ptr, auth_field, len) == 0) {
            for (p = i->ptr + len; p != NULL && *p != '\0';) {
                SKIP_BLANKS(&p);
                p0 = p;
                for (ha = &www_auth[0]; ha->scheme != NULL; ha++) {
                    slen = strlen(ha->scheme);
                    if (strncasecmp(p, ha->scheme, slen) == 0) {
                        p += slen;
                        SKIP_BLANKS(&p);
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
                        SKIP_BLANKS(&p);
                        p = extract_auth_param(p, none_auth_param);
                    } else
                        break;
                }
            }
        }
    }
    return hauth->scheme ? hauth : NULL;
}
