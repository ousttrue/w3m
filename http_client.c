#include "http_client.h"
#include "line_input.h"
#include "display.h"
#include "global.h"
#include <unistd.h>
#include <w3m.h>
#include "form.h"
#include "input_stream.h"
#include "local_cgi.h"
#include "textlist.h"
#include "myctype.h"
#include "indep.h"
#include "proxy.h"

#include <openssl/ssl.h>
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

SSL_CTX* ssl_ctx = NULL;

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

static int same_url_p(struct Url* pu1, struct Url* pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port && (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1)
        && (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

void http_init(struct HttpClient* http, struct Url* base_url, enum UrlOptionFlags flag)
{
    http->has_base_url = false;
    http->flag = flag;
    http->session_count = 0;
    memset(http->message_sessions, 0, sizeof(http->message_sessions));
    http->searchHeader = SearchHeader;
    http->searchHeader_through = true;

    if (base_url) {
        http->message_sessions[0] = (struct HttpMessageSession) {
            .url = *base_url,
        };
        ++http->session_count;
        http->has_base_url = true;
    }
}

struct HttpMessageSession* http_redirect(struct HttpClient* http, const char* target,
    struct Form* post, const char* referer)
{
    const char* u = target;
    enum UrlScheme scheme = getURLScheme(&u);
    struct Url* current = http->session_count == 0 ? NULL : &http->message_sessions[http->session_count - 1].url;
    if (current == NULL && scheme == SCM_UNKNOWN && !ArgvIsURL)
        u = file_to_url(target); /* force to local file */
    else
        u = target;

    struct Url* lastUrl = http->session_count == 0
        ? NULL
        : &http->message_sessions[http->session_count - 1].url;
    struct Url url = parseURL2(u, lastUrl);

    if (http->session_count + 1 >= FollowRedirection) {
        return NULL; // HTTP_REDIRECTION_EXCEEDED;
    }

    for (int i = 0; i < http->session_count; ++i) {
        if (same_url_p(&http->message_sessions[i].url, &url)) {
            return NULL; // HTTP_REDIRECTION_LOOP_DETECTED;
        }
    }

    // if (nredir >= FollowRedirection) {
    //     Str tmp = Sprintf("Number of redirections exceeded %d at %s",
    //         FollowRedirection, parsedURL2Str(pu)->ptr);
    //     disp_err_message(args, tmp->ptr, FALSE);
    //     return FALSE;
    // }

    // if ((same_url_p(pu, &puv[(nredir - 1) % nredir_size]) || (!(nredir % 2) && same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
    //     /* FIXME: gettextize? */
    //     tmp = Sprintf("Redirection loop detected (%s)",
    //         parsedURL2Str(pu)->ptr);
    //     disp_err_message(args, tmp->ptr, FALSE);
    //     return FALSE;
    // }

    http->message_sessions[http->session_count] = (struct HttpMessageSession) {
        .url = url,
        .req = (struct HttpRequest) {
            .http_method = HR_COMMAND_GET,
            .post = post,
            .referer = referer,
            .flag = 0,
        },
        .transport = init_stream((struct Url) { 0 }, NULL),
        .transport_status = HTST_UNKNOWN,

        // response
        .current_content_length = 0,
        .content_charset = 0,
        .res = (struct HttpResponse) { 0 },
        .t = "text/plain",
        .real_type = NULL,
        .extra_header = newTextList(),
        .uname = NULL,
        .pwd = NULL,
        .realm = NULL,
        .add_auth_cookie_flag = false,
        .auth_pu = NULL,
        .page = NULL,
        .charset = WC_CES_US_ASCII,
    };

    // if (!puv) {
    // puv = New_N(struct Url, nredir_size);
    // memset(puv, 0, sizeof(struct Url) * nredir_size);
    // }
    // copyParsedURL(&puv[nredir % nredir_size], pu);

    // return HTTP_REDIRECTION_OK;
    return &http->message_sessions[http->session_count++];
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

static void openLocal(struct HttpMessageSession* m)
{
    struct InputStream* stream;
    if (m->req.post && m->req.post->body)
        stream = ist_from_fp(localcgi_post(m->url.real_file, m->url.query, m->req.post, m->req.referer), fclose);
    else
        stream = ist_from_fp(localcgi_get(m->url.real_file, m->url.query, m->req.referer), fclose);

    if (stream) {
        m->transport = init_stream((struct Url) { 0 }, NULL);
        m->transport.is_cgi = true;
        m->url.scheme = SCM_LOCAL_CGI;
        m->transport.url = m->url;
        return;
    }

    m->transport = examineFile(m->url.real_file);
    if (m->transport.stream) {
        m->transport.url = m->url;
        return;
    }

    if (dir_exist(m->url.real_file)) {
        add_index_file(&m->url, &m->transport);
        m->transport.url = m->url;
        return;
    }

    if (document_root) {
        Str tmp = Strnew_charp(document_root);
        if (Strlastchar(tmp) != '/' && m->url.file[0] != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, m->url.file);
        const char* p = cleanupName(tmp->ptr);
        const char* q = cleanupName(file_unquote(p));
        if (dir_exist(q)) {
            m->url.file = p;
            m->url.real_file = q;
            add_index_file(&m->url, &m->transport);
            m->transport.url = m->url;
            return;
        }

        m->transport = examineFile(q);
        if (m->transport.stream) {
            m->url.file = p;
            m->url.real_file = q;
            m->transport.url = m->url;
        }
        return;
    }

    m->transport = init_stream((struct Url) { 0 }, NULL);
}

static void
SSL_write_from_file(SSL* ssl, const char* file)
{
    FILE* fd = fopen(file, "r");
    if (fd != NULL) {
        int c;
        while ((c = fgetc(fd)) != EOF) {
            char buf[1];
            buf[0] = c;
            SSL_write(ssl, buf, 1);
        }
        fclose(fd);
    }
}

static void
write_from_file(int sock, const char* file)
{
    FILE* fd = fopen(file, "r");
    if (fd != NULL) {
        int c;
        while ((c = fgetc(fd)) != EOF) {
            char buf[1];
            buf[0] = c;
            write(sock, buf, 1);
        }
        fclose(fd);
    }
}

void openHttp(struct CmdArgs* args,
    struct HttpMessageSession* current,
    struct HttpMessageSession* base)
{
    if (current->url.file == NULL)
        current->url.file = allocStr("/", -1);
    if (current->req.post && current->req.post->method == FORM_METHOD_POST && current->req.post->body)
        current->req.http_method = HR_COMMAND_POST;
    if (current->req.post && current->req.post->method == FORM_METHOD_HEAD)
        current->req.http_method = HR_COMMAND_HEAD;

    int sock = 0;
    SSL* sslh = NULL;
    const char* ssl_certificate = NULL;
    Str tmp = NULL;
    if ((
            (current->url.scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) : non_null(HTTP_proxy))
        && use_proxy && current->url.host != NULL && !check_no_proxy(current->url.host)) {
        current->req.flag |= HR_FLAG_PROXY;
        if (current->url.scheme == SCM_HTTPS && current->transport_status == HTST_CONNECT) {
            sock = ist_fd(base->transport.stream);
            if (!(sslh = openSSLHandle(args, sock, current->url.host,
                      &ssl_certificate))) {
                current->transport_status = HTST_MISSING;
                current->transport = init_stream((struct Url) { 0 }, NULL);
                return;
            }
        } else if (current->url.scheme == SCM_HTTPS) {
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
            current->transport = init_stream((struct Url) { 0 }, NULL);
            return;
        }
        if (current->url.scheme == SCM_HTTPS) {
            if (current->transport_status == HTST_NORMAL) {
                current->req.http_method = HR_COMMAND_CONNECT;
                tmp = HTTPrequest(current->url, base ? &base->url : NULL, &current->req, current->extra_header);
                current->transport_status = HTST_CONNECT;
            } else {
                current->req.flag |= HR_FLAG_LOCAL;
                tmp = HTTPrequest(current->url, base ? &base->url : NULL, &current->req, current->extra_header);
                current->transport_status = HTST_NORMAL;
            }
        } else {
            tmp = HTTPrequest(current->url, base ? &base->url : NULL, &current->req, current->extra_header);
            current->transport_status = HTST_NORMAL;
        }
    } else {
        sock = openSocket(current->url.host, schemeToName(current->url.scheme), current->url.port);
        if (sock < 0) {
            current->transport_status = HTST_MISSING;
            current->transport = init_stream((struct Url) { 0 }, NULL);
            return;
        }
        if (current->url.scheme == SCM_HTTPS) {
            if (!(sslh = openSSLHandle(args, sock, current->url.host, &ssl_certificate))) {
                current->transport_status = HTST_MISSING;
                current->transport = init_stream((struct Url) { 0 }, NULL);
                return;
            }
        }
        current->req.flag |= HR_FLAG_LOCAL;
        tmp = HTTPrequest(current->url, base ? &base->url : NULL, &current->req, current->extra_header);
        current->transport_status = HTST_NORMAL;
    }

    struct InputStream* stream = NULL;
    if (current->url.scheme == SCM_HTTPS) {
        if (sslh) {
            SSL_write(sslh, tmp->ptr, tmp->length);
        } else {
            // ?
            write(sock, tmp->ptr, tmp->length);
        }

        if (current->req.http_method == HR_COMMAND_POST && current->req.post->enctype == FORM_ENCTYPE_MULTIPART) {
            if (sslh)
                SSL_write_from_file(sslh, current->req.post->body);
            else
                write_from_file(sock, current->req.post->body);
        }

        stream = ist_from_socket(sock, sslh);
    } else {
        write(sock, tmp->ptr, tmp->length);

        if (current->req.http_method == HR_COMMAND_POST && current->req.post->enctype == FORM_ENCTYPE_MULTIPART)
            write_from_file(sock, current->req.post->body);

        stream = ist_from_socket(sock, 0);
    }

    current->transport = init_stream((struct Url) { 0 }, NULL);
    if (base && base->transport_status == HTST_CONNECT) {
        current->transport = base->transport;
    }
    current->transport.url = current->url;
    current->transport.stream = stream;
    current->transport.ssl_certificate = ssl_certificate;
}

void http_open(struct HttpClient* http, struct CmdArgs* args)
{
    struct HttpMessageSession* current = http_session_current(http);
    struct HttpMessageSession* base = http_session_base(http);

    if (current->url.scheme == SCM_FILE && current->url.file == NULL) {
        if (current->url.label != NULL) {
            /* #hogege is not a label but a filename */
            Str tmp2 = Strnew_charp("#");
            Strcat_charp(tmp2, current->url.label);
            current->url.file = tmp2->ptr;
            current->url.real_file = cleanupName(file_unquote(current->url.file));
            current->url.label = NULL;
        } else {
            /* given URL must be null string */
            return;
        }
    }

    if (LocalhostOnly && current->url.host && !is_localhost(current->url.host))
        current->url.host = NULL;
    current->url.is_nocache = (http->flag & RG_NOCACHE);

    switch (current->url.scheme) {
    case SCM_FILE:
    case SCM_LOCAL_CGI: {
        openLocal(current);
        if (current->transport.stream == NULL && retryAsHttp && current->url.file[0] != '/') {
            if (current->url.scheme == SCM_UNKNOWN) {
                /* retry it as "http://" */
                const char* u = Strnew_m_charp("http://", current->url.file, NULL)->ptr;
                // goto retry;
                current->url = parseURL2(u, NULL);
                openHttp(args, current, base);
            }
        }
        break;
    }

    case SCM_HTTP:
    case SCM_HTTPS: {
        openHttp(args, current, base);
        break;
    }

    default:
        current->transport = init_stream((struct Url) { 0 }, NULL);
        break;
    }
}
