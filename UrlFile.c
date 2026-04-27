#include "UrlFile.h"
#include "line_input.h"
#include "display.h"
#include "form.h"
#include "local_cgi.h"
#include "http_request.h"
#include "global.h"
#include "url.h"
#include "content_type.h"
#include "input_stream.h"
#include "indep.h"
#include "textlist.h"
#include "etc.h"
#include "myctype.h"
#include "proxy.h"
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>

#include <openssl/ssl.h>
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

struct URLFile init_stream(enum UrlScheme scheme, struct InputStream* stream)
{
    return (struct URLFile) {
        // memset(uf, 0, sizeof(struct URLFile));
        .stream = stream,
        .scheme = scheme,
        .encoding = ENC_7BIT,
        .is_cgi = false,
        .compression = CMP_NOCOMPRESS,
        .content_encoding = CMP_NOCOMPRESS,
        .guess_type = NULL,
        .ext = NULL,
        .modtime = -1,
    };
}

static FILE*
lessopen_stream(const char* path)
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

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)

struct URLFile examineFile(const char* path)
{
    struct URLFile uf = init_stream(SCM_FILE, NULL);

    struct stat stbuf;
    if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 || NOT_REGULAR(stbuf.st_mode)) {
        uf.stream = NULL;
        return uf;
    }

    uf.stream = ist_from_path(path);
    if (!do_download) {
        if (use_lessopen && getenv("LESSOPEN") != NULL) {
            uf.guess_type = guessContentType(path);
            if (uf.guess_type == NULL)
                uf.guess_type = "text/plain";
            if (is_html_type(uf.guess_type))
                return uf;
            FILE* fp;
            if ((fp = lessopen_stream(path))) {
                UFclose(&uf);
                uf.stream = ist_from_fp(fp, pclose);
                uf.guess_type = "text/plain";
                return uf;
            }
        }
        check_compression(&uf, path);
        if (uf.compression != CMP_NOCOMPRESS) {
            const char* ext = uf.ext;
            const char* t0 = uncompressed_file_type(path, &ext);
            uf.guess_type = t0;
            uf.ext = ext;
            uncompress_stream(&uf, NULL);
            return uf;
        }
    }

    return uf;
}

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
        *uf = examineFile(q);
        if (uf->stream != NULL) {
            pu->file = p;
            pu->real_file = q;
            return;
        }
    }
}

SSL_CTX* ssl_ctx = NULL;

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

static SSL*
openSSLHandle(struct CmdArgs* args, int sock, const char* hostname, const char** p_cert)
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
        Str serv_cert = ssl_get_certificate(args, handle, hostname);
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
    disp_err_message(args,
        Sprintf("SSL error: %s, a workaround might be: w3m -insecure",
            ERR_error_string(ERR_get_error(), NULL))
            ->ptr,
        false);
    return NULL;
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
struct URLFile
openURL(struct CmdArgs* args, const char* url, struct Url* pu, struct Url* current,
    struct URLOption* option, struct Form* request, TextList* extra_header,
    struct URLFile* ouf, struct HttpRequest* hr, unsigned char* status)
{
    struct HttpRequest hr0;
    if (hr == NULL)
        hr = &hr0;

    struct URLFile uf;
    if (ouf) {
        uf = *ouf;
    } else {
        uf = init_stream(SCM_UNKNOWN, NULL);
    }

    Str tmp;
    int sock;
    const char *p, *q;
    Str gophertmp;
    char type;
    int n;
    SSL* sslh = NULL;

    const char* u = url;
    enum UrlScheme scheme = getURLScheme(&u);
    if (current == NULL && scheme == SCM_UNKNOWN && !ArgvIsURL)
        u = file_to_url(url); /* force to local file */
    else
        u = url;

retry:
    *pu = parseURL2(u, current);
    if (pu->scheme == SCM_FILE && pu->file == NULL) {
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
    case SCM_FILE:
    case SCM_LOCAL_CGI:
        if (request && request->body)
            /* local CGI: POST */
            uf.stream = ist_from_fp(localcgi_post(pu->real_file, pu->query, request, option->referer),
                fclose);
        else
            /* lodal CGI: GET */
            uf.stream = ist_from_fp(localcgi_get(pu->real_file, pu->query, option->referer), fclose);
        if (uf.stream) {
            uf.is_cgi = true;
            uf.scheme = pu->scheme = SCM_LOCAL_CGI;
            return uf;
        }
        uf = examineFile(pu->real_file);
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
                    uf = examineFile(q);
                    if (uf.stream) {
                        pu->file = p;
                        pu->real_file = q;
                    }
                }
            }
        }
        if (uf.stream == NULL && retryAsHttp && url[0] != '/') {
            if (scheme == SCM_UNKNOWN) {
                /* retry it as "http://" */
                u = Strnew_m_charp("http://", url, NULL)->ptr;
                goto retry;
            }
        }
        return uf;
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
                sock = ist_fd(ouf->stream);
                if (!(sslh = openSSLHandle(args, sock, pu->host,
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
                if (!(sslh = openSSLHandle(args, sock, pu->host,
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
            uf.stream = ist_from_socket(sock, sslh);
            if (sslh)
                SSL_write(sslh, tmp->ptr, tmp->length);
            else
                write(sock, tmp->ptr, tmp->length);

            if (hr->http_method == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART) {
                if (sslh)
                    SSL_write_from_file(sslh, request->body);
                else
                    write_from_file(sock, request->body);
            }
            return uf;
        } else {
            write(sock, tmp->ptr, tmp->length);

            if (hr->http_method == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
                write_from_file(sock, request->body);
        }
        break;
    case SCM_UNKNOWN:
    default:
        return uf;
    }
    uf.stream = ist_from_socket(sock, 0);
    return uf;
}
void UFclose(struct URLFile* f)
{
    if (ist_destroy(f->stream)) {
        f->stream = NULL;
    }
}

void UFhalfclose(struct URLFile* f)
{
    switch (f->scheme) {
    default:
        UFclose(f);
        break;
    }
}

static Str accept_this_site;

static void ssl_accept_this_site(const char* hostname)
{
    if (hostname)
        accept_this_site = Strnew_charp(hostname);
    else
        accept_this_site = NULL;
}

void free_ssl_ctx(void)
{
    if (ssl_ctx != NULL)
        SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
    ssl_accept_this_site(NULL);
}

static int
ssl_match_cert_ident(const char* ident, int ilen, const char* hostname)
{
    /* RFC2818 3.1.  Server Identity
     * Names may contain the wildcard
     * character * which is considered to match any single domain name
     * component or component fragment. E.g., *.a.com matches foo.a.com but
     * not bar.foo.a.com. f*.com matches foo.com but not bar.com.
     */
    int hlen = strlen(hostname);
    int i, c;

    /* Is this an exact match? */
    if ((ilen == hlen) && strncasecmp(ident, hostname, hlen) == 0)
        return true;

    for (i = 0; i < ilen; i++) {
        if (ident[i] == '*' && ident[i + 1] == '.') {
            while ((c = *hostname++) != '\0')
                if (c == '.')
                    break;
            i++;
        } else {
            if (ident[i] != *hostname++)
                return false;
        }
    }
    return *hostname == '\0';
}

static Str
ssl_check_cert_ident(X509* x, const char* hostname)
{
    int i;
    Str ret = NULL;
    int match_ident = false;
    /*
     * All we need to do here is check that the CN matches.
     *
     * From RFC2818 3.1 Server Identity:
     * If a subjectAltName extension of type dNSName is present, that MUST
     * be used as the identity. Otherwise, the (most specific) Common Name
     * field in the Subject field of the certificate MUST be used. Although
     * the use of the Common Name is existing practice, it is deprecated and
     * Certification Authorities are encouraged to use the dNSName instead.
     */
    i = X509_get_ext_by_NID(x, NID_subject_alt_name, -1);
    if (i >= 0) {
        X509_EXTENSION* ex;
        STACK_OF(GENERAL_NAME) * alt;

        ex = X509_get_ext(x, i);
        alt = X509V3_EXT_d2i(ex);
        if (alt) {
            int n;
            GENERAL_NAME* gn;
            Str seen_dnsname = NULL;

            n = sk_GENERAL_NAME_num(alt);
            for (i = 0; i < n; i++) {
                gn = sk_GENERAL_NAME_value(alt, i);
                if (gn->type == GEN_DNS) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
                    unsigned char* sn = ASN1_STRING_data(gn->d.ia5);
#else
                    const unsigned char* sn = ASN1_STRING_get0_data(gn->d.ia5);
#endif
                    int sl = ASN1_STRING_length(gn->d.ia5);

                    /*
                     * sn is a pointer to internal data and not guaranteed to
                     * be null terminated. Ensure we have a null terminated
                     * string that we can modify.
                     */
                    char* asn = malloc(sl + 1);
                    if (!asn)
                        exit(1);
                    bcopy(sn, asn, sl);
                    asn[sl] = '\0';

                    if (!seen_dnsname)
                        seen_dnsname = Strnew();
                    /* replace \0 to make full string visible to user */
                    if (sl != strlen(asn)) {
                        int i;
                        for (i = 0; i < sl; ++i) {
                            if (!asn[i])
                                asn[i] = '!';
                        }
                    }
                    Strcat_m_charp(seen_dnsname, asn, " ", NULL);
                    if (sl == strlen(asn) /* catch \0 in SAN */
                        && ssl_match_cert_ident(asn, sl, hostname))
                        break;
                }
            }
            X509V3_EXT_get(ex);
            sk_GENERAL_NAME_free(alt);
            if (i < n) /* Found a match */
                match_ident = true;
            else if (seen_dnsname)
                /* FIXME: gettextize? */
                ret = Sprintf("Bad cert ident from %s: dNSName=%s", hostname,
                    seen_dnsname->ptr);
        }
    }

    if (match_ident == false && ret == NULL) {
        X509_NAME* xn;
        char buf[2048];
        int slen;

        xn = X509_get_subject_name(x);

        slen = X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf));
        if (slen == -1)
            /* FIXME: gettextize? */
            ret = Strnew_charp("Unable to get common name from peer cert");
        else if (slen != strlen(buf)
            || !ssl_match_cert_ident(buf, strlen(buf), hostname)) {
            /* replace \0 to make full string visible to user */
            if (slen != strlen(buf)) {
                int i;
                for (i = 0; i < slen; ++i) {
                    if (!buf[i])
                        buf[i] = '!';
                }
            }
            /* FIXME: gettextize? */
            ret = Sprintf("Bad cert ident %s from %s", buf, hostname);
        }
    }
    return ret;
}

Str ssl_get_certificate(struct CmdArgs* args, SSL* ssl, const char* hostname)
{
    BIO* bp;
    X509* x;
    X509_NAME* xn;
    char* p;
    int len;
    Str s;
    char buf[2048];
    Str amsg = NULL;
    Str emsg;
    const char* ans;

    if (ssl == NULL)
        return NULL;
    x = SSL_get_peer_certificate(ssl);
    if (x == NULL) {
        if (accept_this_site
            && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = "y";
        else {
            /* FIXME: gettextize? */
            emsg = Strnew_charp("No SSL peer certificate: accept? (y/n)");
            ans = inputAnswer(args, emsg->ptr);
        }
        if (ans && TOLOWER(*ans) == 'y')
            /* FIXME: gettextize? */
            amsg = Strnew_charp("Accept SSL session without any peer certificate");
        else {
            /* FIXME: gettextize? */
            char* e = "This SSL session was rejected "
                      "to prevent security violation: no peer certificate";
            disp_err_message(args, e, false);
            free_ssl_ctx();
            return NULL;
        }
        if (amsg)
            disp_err_message(args, amsg->ptr, false);
        ssl_accept_this_site(hostname);
        /* FIXME: gettextize? */
        s = amsg ? amsg : Strnew_charp("valid certificate");
        return s;
    }
    /* check the cert chain.
     * The chain length is automatically checked by OpenSSL when we
     * set the verify depth in the ctx.
     */
    if (ssl_verify_server) {
        long verr;
        if ((verr = SSL_get_verify_result(ssl))
            != X509_V_OK) {
            const char* em = X509_verify_cert_error_string(verr);
            if (accept_this_site
                && strcasecmp(accept_this_site->ptr, hostname) == 0)
                ans = "y";
            else {
                /* FIXME: gettextize? */
                emsg = Sprintf("%s: accept? (y/n)", em);
                ans = inputAnswer(args, emsg->ptr);
            }
            if (ans && TOLOWER(*ans) == 'y') {
                /* FIXME: gettextize? */
                amsg = Sprintf("Accept unsecure SSL session: "
                               "unverified: %s",
                    em);
            } else {
                /* FIXME: gettextize? */
                char* e = Sprintf("This SSL session was rejected: %s", em)->ptr;
                disp_err_message(args, e, false);
                free_ssl_ctx();
                return NULL;
            }
        }
    }
    emsg = ssl_check_cert_ident(x, hostname);
    if (emsg != NULL) {
        if (accept_this_site
            && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = "y";
        else {
            Str ep = Strdup(emsg);
            if (ep->length > COLS - 16)
                Strshrink(ep, ep->length - (COLS - 16));
            Strcat_charp(ep, ": accept? (y/n)");
            ans = inputAnswer(args, ep->ptr);
        }
        if (ans && TOLOWER(*ans) == 'y') {
            /* FIXME: gettextize? */
            amsg = Strnew_charp("Accept unsecure SSL session:");
            Strcat(amsg, emsg);
        } else {
            /* FIXME: gettextize? */
            const char* e = "This SSL session was rejected "
                            "to prevent security violation";
            disp_err_message(args, e, false);
            free_ssl_ctx();
            return NULL;
        }
    }
    if (amsg)
        disp_err_message(args, amsg->ptr, false);
    ssl_accept_this_site(hostname);
    /* FIXME: gettextize? */
    s = amsg ? amsg : Strnew_charp("valid certificate");
    Strcat_charp(s, "\n");
    xn = X509_get_subject_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
        Strcat_charp(s, " subject=<unknown>");
    else
        Strcat_m_charp(s, " subject=", buf, NULL);
    xn = X509_get_issuer_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
        Strcat_charp(s, ": issuer=<unknown>");
    else
        Strcat_m_charp(s, ": issuer=", buf, NULL);
    Strcat_charp(s, "\n\n");

    bp = BIO_new(BIO_s_mem());
    X509_print(bp, x);
    len = (int)BIO_ctrl(bp, BIO_CTRL_INFO, 0, (char*)&p);
    Strcat_charp_n(s, p, len);
    BIO_free_all(bp);
    X509_free(x);
    return s;
}
