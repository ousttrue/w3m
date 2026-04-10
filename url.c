#include "url.h"
#include "term_tty.h"
#include "siteconf.h"
#include "form.h"
#include "terms.h"
#include "istream.h"
#include "indep.h"
#include "buffer.h"
#include "rc.h"
#include "http_request.h"
#include "proxy.h"
#include "signal_util.h"
#include "news.h"
#include "ftp.h"
#include "local.h"
#include "cookie.h"
#include "etc.h"
#include "global.h"
#include "display.h"
#include "proto.h"
#include "html.h"
#include "Str.h"
#include "myctype.h"
#include "regex.h"

#include "wc_util.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>

Str header_string = (NULL);

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

static void add_index_file(struct Url* pu, struct URLFile* uf);

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

SSL_CTX* ssl_ctx = NULL;

void free_ssl_ctx(void)
{
    if (ssl_ctx != NULL)
        SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
    ssl_accept_this_site(NULL);
}

#if SSLEAY_VERSION_NUMBER >= 0x00905100
#include <openssl/rand.h>
static void
init_PRNG(void)
{
    char buffer[256];
    const char* file;
    long l;
    if (RAND_status())
        return;
    if ((file = RAND_file_name(buffer, sizeof(buffer)))) {
#ifdef USE_EGD
        if (RAND_egd(file) > 0)
            return;
#endif
        RAND_load_file(file, -1);
    }
    if (RAND_status())
        goto seeded;
    srand48((long)time(NULL));
    while (!RAND_status()) {
        l = lrand48();
        RAND_seed((unsigned char*)&l, sizeof(long));
    }
seeded:
    if (file)
        RAND_write_file(file);
}
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

#ifdef SSL_CTX_set_min_proto_version
static int
str_to_ssl_version(const char* name)
{
    if (!strcasecmp(name, "all"))
        return 0;
    if (!strcasecmp(name, "none"))
        return 0;
#ifdef TLS1_3_VERSION
    if (!strcasecmp(name, "TLSv1.3"))
        return TLS1_3_VERSION;
#endif
#ifdef TLS1_2_VERSION
    if (!strcasecmp(name, "TLSv1.2"))
        return TLS1_2_VERSION;
#endif
#ifdef TLS1_1_VERSION
    if (!strcasecmp(name, "TLSv1.1"))
        return TLS1_1_VERSION;
#endif
    if (!strcasecmp(name, "TLSv1.0"))
        return TLS1_VERSION;
    if (!strcasecmp(name, "TLSv1"))
        return TLS1_VERSION;
    if (!strcasecmp(name, "SSLv3.0"))
        return SSL3_VERSION;
    if (!strcasecmp(name, "SSLv3"))
        return SSL3_VERSION;
    return -1;
}
#endif /* SSL_CTX_set_min_proto_version */

static SSL*
openSSLHandle(int sock, const char* hostname, const char** p_cert)
{
    SSL* handle = NULL;
    static const char* old_ssl_forbid_method = NULL;
    static int old_ssl_verify_server = -1;

    if (old_ssl_forbid_method != ssl_forbid_method
        && (!old_ssl_forbid_method || !ssl_forbid_method || strcmp(old_ssl_forbid_method, ssl_forbid_method))) {
        old_ssl_forbid_method = ssl_forbid_method;
        ssl_path_modified = 1;
    }
    if (old_ssl_verify_server != ssl_verify_server) {
        old_ssl_verify_server = ssl_verify_server;
        ssl_path_modified = 1;
    }
    if (ssl_path_modified) {
        free_ssl_ctx();
        ssl_path_modified = 0;
    }
    if (ssl_ctx == NULL) {
        int option;
#if OPENSSL_VERSION_NUMBER < 0x0800
        ssl_ctx = SSL_CTX_new();
        X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
        SSLeay_add_ssl_algorithms();
        SSL_load_error_strings();
#else
        OPENSSL_init_ssl(0, NULL);
#endif
        if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
            goto eend;
#ifdef SSL_CTX_set_min_proto_version
        if (ssl_min_version && *ssl_min_version != '\0') {
            int sslver;
            sslver = str_to_ssl_version(ssl_min_version);
            if (sslver < 0
                || !SSL_CTX_set_min_proto_version(ssl_ctx, sslver)) {
                free_ssl_ctx();
                goto eend;
            }
        }
#endif
        if (ssl_cipher && *ssl_cipher != '\0')
            if (!SSL_CTX_set_cipher_list(ssl_ctx, ssl_cipher)) {
                free_ssl_ctx();
                goto eend;
            }
        option = SSL_OP_ALL;
        if (ssl_forbid_method) {
            if (strchr(ssl_forbid_method, '2'))
                option |= SSL_OP_NO_SSLv2;
            if (strchr(ssl_forbid_method, '3'))
                option |= SSL_OP_NO_SSLv3;
            if (strchr(ssl_forbid_method, 't'))
                option |= SSL_OP_NO_TLSv1;
            if (strchr(ssl_forbid_method, 'T'))
                option |= SSL_OP_NO_TLSv1;
            if (strchr(ssl_forbid_method, '4'))
                option |= SSL_OP_NO_TLSv1;
#ifdef SSL_OP_NO_TLSv1_1
            if (strchr(ssl_forbid_method, '5'))
                option |= SSL_OP_NO_TLSv1_1;
#endif
#ifdef SSL_OP_NO_TLSv1_2
            if (strchr(ssl_forbid_method, '6'))
                option |= SSL_OP_NO_TLSv1_2;
#endif
#ifdef SSL_OP_NO_TLSv1_3
            if (strchr(ssl_forbid_method, '7'))
                option |= SSL_OP_NO_TLSv1_3;
#endif
        }
#ifdef SSL_OP_NO_COMPRESSION
        option |= SSL_OP_NO_COMPRESSION;
#endif
        SSL_CTX_set_options(ssl_ctx, option);

#ifdef SSL_MODE_RELEASE_BUFFERS
        SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

        /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
        SSL_CTX_set_verify(ssl_ctx,
            ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
        if (ssl_cert_file != NULL && *ssl_cert_file != '\0') {
            int ng = 1;
            if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file, SSL_FILETYPE_PEM) > 0) {
                const char* key_file = (ssl_key_file == NULL
                                           || *ssl_key_file == '\0')
                    ? ssl_cert_file
                    : ssl_key_file;
                if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) > 0)
                    if (SSL_CTX_check_private_key(ssl_ctx))
                        ng = 0;
            }
            if (ng) {
                free_ssl_ctx();
                goto eend;
            }
        }
        if (ssl_verify_server) {
            const char *file = NULL, *path = NULL;
            if (ssl_ca_file && *ssl_ca_file != '\0')
                file = ssl_ca_file;
            if (ssl_ca_path && *ssl_ca_path != '\0')
                path = ssl_ca_path;
            if ((file || path)
                && !SSL_CTX_load_verify_locations(ssl_ctx, file, path)) {
                free_ssl_ctx();
                goto eend;
            }
            if (ssl_ca_default)
                SSL_CTX_set_default_verify_paths(ssl_ctx);
        }
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */
    }
    handle = SSL_new(ssl_ctx);
    SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
    init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
    SSL_set_tlsext_host_name(handle, hostname);
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) */
    if (SSL_connect(handle) > 0) {
        Str serv_cert = ssl_get_certificate(handle, hostname);
        if (serv_cert) {
            *p_cert = serv_cert->ptr;
            return handle;
        }
        close(sock);
        SSL_free(handle);
        return NULL;
    }
eend:
    close(sock);
    if (handle)
        SSL_free(handle);
    /* FIXME: gettextize? */
    disp_err_message(Sprintf("SSL error: %s, a workaround might be: w3m -insecure",
                         ERR_error_string(ERR_get_error(), NULL))
                         ->ptr,
        false);
    return NULL;
}

static void
SSL_write_from_file(SSL* ssl, const char* file)
{
    FILE* fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL) {
        while ((c = fgetc(fd)) != EOF) {
            buf[0] = c;
            SSL_write(ssl, buf, 1);
        }
        fclose(fd);
    }
}

static void
write_from_file(int sock, const char* file)
{
    FILE* fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL) {
        while ((c = fgetc(fd)) != EOF) {
            buf[0] = c;
            write(sock, buf, 1);
        }
        fclose(fd);
    }
}

struct Url*
baseURL(struct Buffer* buf)
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

int openSocket(const char* const hostname,
    const char* remoteport_name, unsigned short remoteport_num)
{
    volatile int sock = -1;
    int* af;
    struct addrinfo hints, *res0, *res;
    int error;
    const char* hname;
    SignalFunc prevtrap = NULL;

    if (fmInitialized) {
        /* FIXME: gettextize? */
        message(Sprintf("Opening socket...")->ptr, 0, 0);
        refresh();
    }
    if (SETJMP(AbortLoading) != 0) {
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
    hname = hostname;
    if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
        hname = allocStr(hostname + 1, -1);
        ((char*)hname)[strlen(hname) - 1] = '\0';
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

struct Url parseURL(const char* url, const struct Url* current)
{
    url = url_quote(url); /* quote 0x01-0x20, 0x7F-0xFF */

    const char* p = url;

    struct Url p_url = { 0 };
    // copyParsedURL(p_url, NULL);
    p_url.scheme = SCM_MISSING;

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    if (*url == '\0' || *url == '#') {
        if (current)
            copyParsedURL(&p_url, current);
        goto do_label;
    }
    if (IS_ALPHA(*p) && (p[1] == ':' || p[1] == '|')) {
        p_url.scheme = SCM_LOCAL;
        goto analyze_file;
    }

    /* search for scheme */
    p_url.scheme = getURLScheme(&p);
    if (p_url.scheme == SCM_MISSING) {
        /* scheme part is not found in the url. This means either
         * (a) the url is relative to the current or (b) the url
         * denotes a filename (therefore the scheme is SCM_LOCAL).
         */
        if (current) {
            switch (current->scheme) {
            case SCM_LOCAL:
            case SCM_LOCAL_CGI:
                p_url.scheme = SCM_LOCAL;
                break;
            case SCM_FTP:
            case SCM_FTPDIR:
                p_url.scheme = SCM_FTP;
                break;
            case SCM_NNTP:
            case SCM_NNTP_GROUP:
                p_url.scheme = SCM_NNTP;
                break;
            case SCM_NEWS:
            case SCM_NEWS_GROUP:
                p_url.scheme = SCM_NEWS;
                break;
            default:
                p_url.scheme = current->scheme;
                break;
            }
        } else
            p_url.scheme = SCM_LOCAL;
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
    if (p_url.scheme == SCM_UNKNOWN) {
        p_url.file = allocStr(url, -1);
        return p_url;
    }
    /* get host and port */
    if (p[0] != '/' || p[1] != '/') { /* scheme:foo or scheme:/foo */
        p_url.host = NULL;
        p_url.port = getDefaultPort(p_url.scheme);
        goto analyze_file;
    }
    /* after here, p begins with // */
    if (p_url.scheme == SCM_LOCAL) { /* file://foo           */
        if (p[2] == '/' || p[2] == '~'
            /* <A HREF="file:///foo">file:///foo</A>  or <A HREF="file://~user">file://~user</A> */
            || (IS_ALPHA(p[2]) && (p[3] == ':' || p[3] == '|'))
            /* <A HREF="file://DRIVE/foo">file://DRIVE/foo</A> */
        ) {
            p += 2;
            goto analyze_file;
        }
    }
    p += 2; /* scheme://foo         */
    /*          ^p is here  */

    const char *q, *qq;
    Str tmp;

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
            p_url.user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
            p_url.pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
            p++;
            goto analyze_url;
        }
        /* scheme://host:port/ */
        p_url.host = copyPath(qq, q - 1 - qq,
            COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
        tmp = Strnew_charp_n(q, p - q);
        p_url.port = atoi(tmp->ptr);
        /* *p is one of ['\0', '/', '?', '#'] */
        break;
    case '@':
        /* scheme://user@...            */
        p_url.user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
        p++;
        goto analyze_url;
    case '\0':
        /* scheme://host                */
    case '/':
    case '?':
    case '#':
        p_url.host = copyPath(q, p - q, COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
        p_url.port = getDefaultPort(p_url.scheme);
        break;
    }
analyze_file:
    if (p_url.scheme == SCM_LOCAL && p_url.user == NULL && p_url.host != NULL && *p_url.host != '\0' && !is_localhost(p_url.host)) {
        /*
         * In the environments other than CYGWIN, a URL like
         * file://host/file is regarded as ftp://host/file.
         * On the other hand, file://host/file on CYGWIN is
         * regarded as local access to the file //host/file.
         * `host' is a netbios-hostname, drive, or any other
         * name; It is CYGWIN system call who interprets that.
         */

        p_url.scheme = SCM_FTP; /* ftp://host/... */
        if (p_url.port == 0)
            p_url.port = getDefaultPort(SCM_FTP);
    }
    if ((*p == '\0' || *p == '#' || *p == '?') && p_url.host == NULL) {
        p_url.file = "";
        goto do_query;
    }
    if (p_url.scheme == SCM_LOCAL) {
        q = p;
        if (*q == '/')
            q++;
        if (IS_ALPHA(q[0]) && (q[1] == ':' || q[1] == '|')) {
            if (q[1] == '|') {
                p = allocStr(q, -1);
                ((char*)p)[1] = ':';
            } else
                p = q;
        }
    }

    q = p;
    if (p_url.scheme == SCM_GOPHER) {
        if (*q == '/')
            q++;
        if (*q && q[0] != '/' && q[1] != '/' && q[2] == '/')
            q++;
    }
    if (*p == '/')
        p++;
    if (*p == '\0' || *p == '#' || *p == '?') { /* scheme://host[:port]/ */
        p_url.file = DefaultFile(p_url.scheme);
        goto do_query;
    }
    if (p_url.scheme == SCM_GOPHER && *p == 'R') {
        if (!*++p) {
            p_url.file = "";
            goto do_query;
        }
        tmp = Strnew();
        Strcat_char(tmp, *(p++));
        while (*p && *p != '/')
            p++;
        Strcat_charp(tmp, p);
        while (*p)
            p++;
        p_url.file = copyPath(tmp->ptr, -1, COPYPATH_SPC_IGNORE);
    } else {
        const char* cgi = strchr(p, '?');
    again:
        while (*p && *p != '#' && p != cgi)
            p++;
        if (*p == '#' && p_url.scheme == SCM_LOCAL) {
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
        if (p_url.scheme == SCM_LOCAL || p_url.scheme == SCM_MISSING)
            p_url.file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
        else
            p_url.file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }

do_query:
    if (*p == '?') {
        q = ++p;
        while (*p && *p != '#')
            p++;
        p_url.query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    }
do_label:
    if (p_url.scheme == SCM_MISSING) {
        p_url.scheme = SCM_LOCAL;
        p_url.file = allocStr(p, -1);
        p_url.label = NULL;
    } else if (*p == '#')
        p_url.label = allocStr(p + 1, -1);
    else
        p_url.label = NULL;

    return p_url;
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

struct Url parseURL2(const char* url, const struct Url* current)
{
    struct Url pu = parseURL(url, current);
    if (pu.scheme == SCM_MAILTO)
        return pu;
    if (pu.scheme == SCM_DATA)
        return pu;

    const char* p;
    if (pu.scheme == SCM_NEWS || pu.scheme == SCM_NEWS_GROUP) {
        if (pu.file && !strchr(pu.file, '@') && (!(p = strchr(pu.file, '/')) || strchr(p + 1, '-') || *(p + 1) == '\0'))
            pu.scheme = SCM_NEWS_GROUP;
        else
            pu.scheme = SCM_NEWS;
        return pu;
    }
    if (pu.scheme == SCM_NNTP || pu.scheme == SCM_NNTP_GROUP) {
        if (pu.file && *pu.file == '/')
            pu.file = allocStr(pu.file + 1, -1);
        if (pu.file && !strchr(pu.file, '@') && (!(p = strchr(pu.file, '/')) || strchr(p + 1, '-') || *(p + 1) == '\0'))
            pu.scheme = SCM_NNTP_GROUP;
        else
            pu.scheme = SCM_NNTP;
        if (current && (current->scheme == SCM_NNTP || current->scheme == SCM_NNTP_GROUP)) {
            if (pu.host == NULL) {
                pu.host = current->host;
                pu.port = current->port;
            }
        }
        return pu;
    }
    if (pu.scheme == SCM_LOCAL) {
        char* q = expandName(file_unquote(pu.file));
        Str drive;
        if (IS_ALPHA(q[0]) && q[1] == ':') {
            drive = Strnew_charp_n(q, 2);
            Strcat_charp(drive, file_quote(q + 2));
            pu.file = drive->ptr;
        } else
            pu.file = file_quote(q);
    }

    bool relative_uri = false;
    if (current && (pu.scheme == current->scheme || (pu.scheme == SCM_FTP && current->scheme == SCM_FTPDIR) || (pu.scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI))
        && pu.host == NULL) {
        /* Copy omitted element from the current URL */
        pu.user = current->user;
        pu.pass = current->pass;
        pu.host = current->host;
        pu.port = current->port;
        if (pu.file && *pu.file) {
            if (
                pu.scheme != SCM_GOPHER && pu.file[0] != '/'
                && !(pu.scheme == SCM_LOCAL && IS_ALPHA(pu.file[0])
                    && pu.file[1] == ':')) {
                /* file is relative [process 1] */
                p = pu.file;
                if (current->file) {
                    Str tmp = Strnew_charp(current->file);
                    while (tmp->length > 0) {
                        if (Strlastchar(tmp) == '/')
                            break;
                        Strshrink(tmp, 1);
                    }
                    Strcat_charp(tmp, p);
                    pu.file = tmp->ptr;
                    relative_uri = true;
                }
            } else if (pu.scheme == SCM_GOPHER && pu.file[0] == '/') {
                p = pu.file;
                pu.file = allocStr(p + 1, -1);
            }
        } else { /* scheme:[?query][#label] */
            pu.file = current->file;
            if (!pu.query)
                pu.query = current->query;
        }
        /* comment: query part need not to be completed
         * from the current URL. */
    }
    if (pu.file) {
        if (pu.scheme == SCM_LOCAL && pu.file[0] != '/' &&
#ifdef SUPPORT_DOS_DRIVE_PREFIX /* for 'drive:' */
            !(IS_ALPHA(pu.file[0]) && pu.file[1] == ':') &&
#endif
            strcmp(pu.file, "-")) {
            /* local file, relative path */
            Str tmp = Strnew_charp(CurrentDir);
            if (Strlastchar(tmp) != '/')
                Strcat_char(tmp, '/');
            Strcat_charp(tmp, file_unquote(pu.file));
            pu.file = file_quote(cleanupName(tmp->ptr));
        } else if (pu.scheme == SCM_HTTP
            || pu.scheme == SCM_HTTPS) {
            if (relative_uri) {
                /* In this case, pu.file is created by [process 1] above.
                 * pu.file may contain relative path (for example,
                 * "/foo/../bar/./baz.html"), cleanupName() must be applied.
                 * When the entire abs_path is given, it still may contain
                 * elements like `//', `..' or `.' in the pu.file. It is
                 * server's responsibility to canonicalize such path.
                 */
                pu.file = cleanupName(pu.file);
            }
        } else if (
            pu.scheme != SCM_GOPHER && pu.file[0] == '/') {
            /*
             * this happens on the following conditions:
             * (1) ftp scheme (2) local, looks like absolute path.
             * In both case, there must be no side effect with
             * cleanupName(). (I hope so...)
             */
            pu.file = cleanupName(pu.file);
        }
        if (pu.scheme == SCM_LOCAL) {
            pu.real_file = cleanupName(file_unquote(pu.file));
        }
    }

    return pu;
}

Str _parsedURL2Str(struct Url* pu, bool pass, bool user, bool label)
{
    Str tmp;
    ;

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
    tmp = Strnew_charp(schemeToStr(pu->scheme));
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
        if (pu->port != getDefaultPort(pu->scheme)) {
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

struct URLFile
openURL(const char* url, struct Url* pu, struct Url* current,
    struct URLOption* option, struct Form* request, TextList* extra_header,
    struct URLFile* ouf, struct HttpRequest* hr, unsigned char* status)
{
    Str tmp;
    int sock, scheme;
    const char *p, *q, *u;
    Str gophertmp;
    char type;
    int n;
    struct URLFile uf;
    struct HttpRequest hr0;
    SSL* sslh = NULL;

    if (hr == NULL)
        hr = &hr0;

    if (ouf) {
        uf = *ouf;
    } else {
        init_stream(&uf, SCM_MISSING, NULL);
    }

    u = url;
    scheme = getURLScheme(&u);
    if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL)
        u = file_to_url(url); /* force to local file */
    else
        u = url;
retry:
    *pu = parseURL2(u, current);
    if (pu->scheme == SCM_LOCAL && pu->file == NULL) {
        if (pu->label != NULL) {
            /* #hogege is not a label but a filename */
            Str tmp2 = Strnew_charp("#");
            Strcat_charp(tmp2, pu->label);
            pu->file = tmp2->ptr;
            pu->real_file = cleanupName(file_unquote(pu->file));
            pu->label = NULL;
        } else {
            /* given URL must be null string */
#ifdef SOCK_DEBUG
            sock_log("given URL must be null string\n");
#endif
            return uf;
        }
    }

    if (LocalhostOnly && pu->host && !is_localhost(pu->host))
        pu->host = NULL;

    uf.scheme = pu->scheme;
    uf.url = parsedURL2Str(pu)->ptr;
    pu->is_nocache = (option->flag & RG_NOCACHE);
    uf.ext = filename_extension(pu->file, 1);

    hr->http_method = HR_COMMAND_GET;
    hr->flag = 0;
    hr->referer = option->referer;
    hr->request = request;

    switch (pu->scheme) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
        if (request && request->body)
            /* local CGI: POST */
            uf.stream = newFileStream(localcgi_post(pu->real_file, pu->query,
                                          request, option->referer),
                (void (*)())fclose);
        else
            /* lodal CGI: GET */
            uf.stream = newFileStream(localcgi_get(pu->real_file, pu->query,
                                          option->referer),
                (void (*)())fclose);
        if (uf.stream) {
            uf.is_cgi = true;
            uf.scheme = pu->scheme = SCM_LOCAL_CGI;
            return uf;
        }
        examineFile(pu->real_file, &uf);
        if (uf.stream == NULL) {
            if (dir_exist(pu->real_file)) {
                add_index_file(pu, &uf);
                if (uf.stream == NULL)
                    return uf;
            } else if (document_root != NULL) {
                tmp = Strnew_charp(document_root);
                if (Strlastchar(tmp) != '/' && pu->file[0] != '/')
                    Strcat_char(tmp, '/');
                Strcat_charp(tmp, pu->file);
                p = cleanupName(tmp->ptr);
                q = cleanupName(file_unquote(p));
                if (dir_exist(q)) {
                    pu->file = p;
                    pu->real_file = q;
                    add_index_file(pu, &uf);
                    if (uf.stream == NULL) {
                        return uf;
                    }
                } else {
                    examineFile(q, &uf);
                    if (uf.stream) {
                        pu->file = p;
                        pu->real_file = q;
                    }
                }
            }
        }
        if (uf.stream == NULL && retryAsHttp && url[0] != '/') {
            if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
                /* retry it as "http://" */
                u = Strnew_m_charp("http://", url, NULL)->ptr;
                goto retry;
            }
        }
        return uf;
    case SCM_FTP:
    case SCM_FTPDIR:
        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (non_null(FTP_proxy) && use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
            hr->flag |= HR_FLAG_PROXY;
            sock = openSocket(FTP_proxy_parsed.host,
                schemeToName(FTP_proxy_parsed.scheme),
                FTP_proxy_parsed.port);
            if (sock < 0)
                return uf;
            uf.scheme = SCM_HTTP;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            write(sock, tmp->ptr, tmp->length);
        } else {
            uf.stream = openFTPStream(pu, &uf);
            uf.scheme = pu->scheme;
            return uf;
        }
        break;
    case SCM_HTTP:
    case SCM_HTTPS:
        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (request && request->method == FORM_METHOD_POST && request->body)
            hr->http_method = HR_COMMAND_POST;
        if (request && request->method == FORM_METHOD_HEAD)
            hr->http_method = HR_COMMAND_HEAD;
        if ((
                (pu->scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) : non_null(HTTP_proxy))
            && use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
            hr->flag |= HR_FLAG_PROXY;
            if (pu->scheme == SCM_HTTPS && *status == HTST_CONNECT) {
                sock = ssl_socket_of(ouf->stream);
                if (!(sslh = openSSLHandle(sock, pu->host,
                          &uf.ssl_certificate))) {
                    *status = HTST_MISSING;
                    return uf;
                }
            } else if (pu->scheme == SCM_HTTPS) {
                sock = openSocket(HTTPS_proxy_parsed.host,
                    schemeToName(HTTPS_proxy_parsed.scheme),
                    HTTPS_proxy_parsed.port);
                sslh = NULL;
            } else {
                sock = openSocket(HTTP_proxy_parsed.host,
                    schemeToName(HTTP_proxy_parsed.scheme),
                    HTTP_proxy_parsed.port);
                sslh = NULL;
            }
            if (sock < 0) {
#ifdef SOCK_DEBUG
                sock_log("Can't open socket\n");
#endif
                return uf;
            }
            if (pu->scheme == SCM_HTTPS) {
                if (*status == HTST_NORMAL) {
                    hr->http_method = HR_COMMAND_CONNECT;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_CONNECT;
                } else {
                    hr->flag |= HR_FLAG_LOCAL;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_NORMAL;
                }
            } else {
                tmp = HTTPrequest(pu, current, hr, extra_header);
                *status = HTST_NORMAL;
            }
        } else {
            sock = openSocket(pu->host, schemeToName(pu->scheme), pu->port);
            if (sock < 0) {
                *status = HTST_MISSING;
                return uf;
            }
            if (pu->scheme == SCM_HTTPS) {
                if (!(sslh = openSSLHandle(sock, pu->host,
                          &uf.ssl_certificate))) {
                    *status = HTST_MISSING;
                    return uf;
                }
            }
            hr->flag |= HR_FLAG_LOCAL;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            *status = HTST_NORMAL;
        }
        if (pu->scheme == SCM_HTTPS) {
            uf.stream = newSSLStream(sslh, sock);
            if (sslh)
                SSL_write(sslh, tmp->ptr, tmp->length);
            else
                write(sock, tmp->ptr, tmp->length);
            if (w3m_reqlog) {
                FILE* ff = fopen(w3m_reqlog, "a");
                if (ff == NULL)
                    return uf;
                if (sslh)
                    fputs("HTTPS: request via SSL\n", ff);
                else
                    fputs("HTTPS: request without SSL\n", ff);
                fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                fclose(ff);
            }
            if (hr->http_method == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART) {
                if (sslh)
                    SSL_write_from_file(sslh, request->body);
                else
                    write_from_file(sock, request->body);
            }
            return uf;
        } else {
            write(sock, tmp->ptr, tmp->length);
            if (w3m_reqlog) {
                FILE* ff = fopen(w3m_reqlog, "a");
                if (ff == NULL)
                    return uf;
                fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                fclose(ff);
            }
            if (hr->http_method == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
                write_from_file(sock, request->body);
        }
        break;
    case SCM_GOPHER:
        p = pu->file;
        n = 0;
        while (*p == '/') {
            ++p;
            ++n;
        }
        if (*p != '\0') {
            type = pu->file[n];
            switch (type) {
            case '0':
            case '1':
            case 'm':
            case 's':
            case 'g':
            case 'h':
            case 'I':
            case '5':
            case '7':
            case '9':
                tmp = Strnew_charp(pu->file);
                gophertmp = Strdup(tmp);
                Strdelete(tmp, n, 1);
                pu->file = tmp->ptr;
                break;
            default:
                type = '\0';
                break;
            }
        } else {
            type = '\0';
        }
        if (pu->query != NULL) {
            tmp = Strnew_charp(pu->file);
            Strcat_char(tmp, '\t');
            Strcat_charp(tmp, pu->query);
            pu->file = tmp->ptr;
        }
        if (non_null(GOPHER_proxy) && use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
            hr->flag |= HR_FLAG_PROXY;
            sock = openSocket(GOPHER_proxy_parsed.host,
                schemeToName(GOPHER_proxy_parsed.scheme),
                GOPHER_proxy_parsed.port);
            if (sock < 0)
                return uf;
            uf.scheme = SCM_HTTP;
            tmp = HTTPrequest(pu, current, hr, extra_header);
        } else {
            sock = openSocket(pu->host, schemeToName(pu->scheme), pu->port);
            if (sock < 0)
                return uf;
            if (pu->file == NULL)
                pu->file = "1";
            tmp = Strnew_charp(file_unquote(pu->file));
            Strcat_char(tmp, '\n');
        }
        write(sock, tmp->ptr, tmp->length);
        if (type != '\0') {
            pu->file = gophertmp->ptr;
        }
        break;
    case SCM_NNTP:
    case SCM_NNTP_GROUP:
    case SCM_NEWS:
    case SCM_NEWS_GROUP:
        if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NEWS)
            uf.scheme = SCM_NEWS;
        else
            uf.scheme = SCM_NEWS_GROUP;
        uf.stream = openNewsStream(pu);
        return uf;
    case SCM_DATA:
        if (pu->file == NULL)
            return uf;
        p = Strnew_charp(pu->file)->ptr;
        q = strchr(p, ',');
        if (q == NULL)
            return uf;
        *(char*)q++ = '\0';
        tmp = Strnew_charp(q);
        q = strrchr(p, ';');
        if (q != NULL && !strcmp(q, ";base64")) {
            *(char*)q = '\0';
            uf.encoding = ENC_BASE64;
        } else
            tmp = Str_url_unquote(tmp, false, false);
        uf.stream = newStrStream(tmp);
        uf.guess_type = (*p != '\0') ? p : "text/plain";
        return uf;
    case SCM_UNKNOWN:
    default:
        return uf;
    }
    uf.stream = newInputStream(sock);
    return uf;
}

/* add index_file if exists */
static void
add_index_file(struct Url* pu, struct URLFile* uf)
{
    char *p, *q;
    TextList* index_file_list = NULL;
    TextListItem* ti;

    if (non_null(index_file))
        index_file_list = make_domain_list(index_file);
    if (index_file_list == NULL) {
        uf->stream = NULL;
        return;
    }
    for (ti = index_file_list->first; ti; ti = ti->next) {
        p = Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
        p = cleanupName(p);
        q = cleanupName(file_unquote(p));
        examineFile(q, uf);
        if (uf->stream != NULL) {
            pu->file = p;
            pu->real_file = q;
            return;
        }
    }
}

const char* filename_extension(const char* path, int is_url)
{
    const char *last_dot = "", *p = path;
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
#ifdef DEBUG
    default:
        abort();
#endif
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
        pu_buf = parseURL2(url, base);
        pu = &pu_buf;
    } else {
        pu = base;
    }
    if (pu && (pu->scheme == SCM_LOCAL || pu->scheme == SCM_LOCAL_CGI))
        return SystemCharset;
    csptr = query_SCONF_URL_CHARSET(pu);
    return (csptr && *csptr) ? *csptr : doc_charset ? doc_charset
                                                    : DocumentCharset;
}

char* url_encode(const char* url, const struct Url* base, wc_ces doc_charset)
{
    return url_quote_conv((char*)url,
        url_to_charset(url, base, doc_charset));
}

#if 0 /* unused */
char *
url_decode(const char *url, const struct Url *base, wc_ces doc_charset)
{
    if (!DecodeURL)
	return (char *)url;
    return url_unquote_conv((char *)url,
			    url_to_charset(url, base, doc_charset));
}
#endif

char* url_decode2(const char* url, const struct Buffer* buf)
{
    wc_ces url_charset;

    if (!DecodeURL)
        return (char*)url;
    url_charset = buf ? url_to_charset(url, baseURL((struct Buffer*)buf), buf->document_charset) : url_to_charset(url, NULL, 0);
    return url_unquote_conv((char*)url, url_charset);
}
