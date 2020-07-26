#include "urlfile.h"
#include "types.h"
#include "url.h"
#include "http_request.h"
#include "etc.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "form.h"
#include "display.h"
#include <assert.h>

/* add index_file if exists */
static void
add_index_file(ParsedURL *pu, URLFile *uf)
{
    char *p, *q;
    TextList *index_file_list = NULL;
    TextListItem *ti;

    if (non_null(index_file))
        index_file_list = make_domain_list(index_file);
    if (index_file_list == NULL)
    {
        uf->stream = NULL;
        return;
    }
    for (ti = index_file_list->first; ti; ti = ti->next)
    {
        p = Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
        p = cleanupName(p);
        q = cleanupName(file_unquote(p));
        examineFile(q, uf);
        if (uf->stream != NULL)
        {
            pu->file = p;
            pu->real_file = q;
            return;
        }
    }
}

static void
write_from_file(int sock, char *file)
{
    FILE *fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL)
    {
        while ((c = fgetc(fd)) != EOF)
        {
            buf[0] = c;
            write(sock, buf, 1);
        }
        fclose(fd);
    }
}

SSL_CTX *ssl_ctx = NULL;

void free_ssl_ctx()
{
    if (ssl_ctx != NULL)
        SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
    ssl_accept_this_site(NULL);
}

#ifdef USE_SSL
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>
#endif

#if SSLEAY_VERSION_NUMBER >= 0x00905100
#include <openssl/rand.h>
static void
init_PRNG()
{
    char buffer[256];
    const char *file;
    long l;
    if (RAND_status())
        return;
    if ((file = RAND_file_name(buffer, sizeof(buffer))))
    {
#ifdef USE_EGD
        if (RAND_egd(file) > 0)
            return;
#endif
        RAND_load_file(file, -1);
    }
    if (RAND_status())
        goto seeded;
    srand48((long)time(NULL));
    while (!RAND_status())
    {
        l = lrand48();
        RAND_seed((unsigned char *)&l, sizeof(long));
    }
seeded:
    if (file)
        RAND_write_file(file);
}
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

static SSL *
openSSLHandle(int sock, char *hostname, char **p_cert)
{
    SSL *handle = NULL;
    static char *old_ssl_forbid_method = NULL;
#ifdef USE_SSL_VERIFY
    static int old_ssl_verify_server = -1;
#endif

    if (old_ssl_forbid_method != ssl_forbid_method && (!old_ssl_forbid_method || !ssl_forbid_method ||
                                                       strcmp(old_ssl_forbid_method, ssl_forbid_method)))
    {
        old_ssl_forbid_method = ssl_forbid_method;
#ifdef USE_SSL_VERIFY
        ssl_path_modified = 1;
#else
        free_ssl_ctx();
#endif
    }
#ifdef USE_SSL_VERIFY
    if (old_ssl_verify_server != ssl_verify_server)
    {
        old_ssl_verify_server = ssl_verify_server;
        ssl_path_modified = 1;
    }
    if (ssl_path_modified)
    {
        free_ssl_ctx();
        ssl_path_modified = 0;
    }
#endif /* defined(USE_SSL_VERIFY) */
    if (ssl_ctx == NULL)
    {
        int option;
#if SSLEAY_VERSION_NUMBER < 0x0800
        ssl_ctx = SSL_CTX_new();
        X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
        SSLeay_add_ssl_algorithms();
        SSL_load_error_strings();
        if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
            goto eend;
        option = SSL_OP_ALL;
        if (ssl_forbid_method)
        {
            if (strchr(ssl_forbid_method, '2'))
                option |= SSL_OP_NO_SSLv2;
            if (strchr(ssl_forbid_method, '3'))
                option |= SSL_OP_NO_SSLv3;
            if (strchr(ssl_forbid_method, 't'))
                option |= SSL_OP_NO_TLSv1;
            if (strchr(ssl_forbid_method, 'T'))
                option |= SSL_OP_NO_TLSv1;
        }
        SSL_CTX_set_options(ssl_ctx, option);
#ifdef USE_SSL_VERIFY
        /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
        SSL_CTX_set_verify(ssl_ctx,
                           ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
        if (ssl_cert_file != NULL && *ssl_cert_file != '\0')
        {
            int ng = 1;
            if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file, SSL_FILETYPE_PEM) > 0)
            {
                char *key_file = (ssl_key_file == NULL || *ssl_key_file ==
                                                              '\0')
                                     ? ssl_cert_file
                                     : ssl_key_file;
                if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) > 0)
                    if (SSL_CTX_check_private_key(ssl_ctx))
                        ng = 0;
            }
            if (ng)
            {
                free_ssl_ctx();
                goto eend;
            }
        }
        if ((!ssl_ca_file && !ssl_ca_path) || SSL_CTX_load_verify_locations(ssl_ctx, ssl_ca_file, ssl_ca_path))
#endif /* defined(USE_SSL_VERIFY) */
            SSL_CTX_set_default_verify_paths(ssl_ctx);
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
    if (SSL_connect(handle) > 0)
    {
        Str serv_cert = ssl_get_certificate(handle, hostname);
        if (serv_cert)
        {
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
    disp_err_message(Sprintf("SSL error: %s",
                             ERR_error_string(ERR_get_error(), NULL))
                         ->ptr,
                     FALSE);
    return NULL;
}

static void
SSL_write_from_file(SSL *ssl, char *file)
{
    FILE *fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL)
    {
        while ((c = fgetc(fd)) != EOF)
        {
            buf[0] = c;
            SSL_write(ssl, buf, 1);
        }
        fclose(fd);
    }
}

URLFile::URLFile(SchemaTypes scm, InputStream *strm)
    : scheme(scm), stream(strm)
{
}

void URLFile::openURL(char *url, ParsedURL *pu, ParsedURL *current,
                      URLOption *option, FormList *request, TextList *extra_header,
                      HRequest *hr, unsigned char *status)
{
    Str tmp;
    int sock, scheme;
    char *p, *q, *u;
    HRequest hr0;
    SSL *sslh = NULL;

    u = url;
    scheme = getURLScheme(&u);
    if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL)
        u = file_to_url(url); /* force to local file */
    else
        u = url;
retry:
    pu->Parse2(u, current);
    if (pu->scheme == SCM_LOCAL && pu->file == NULL)
    {
        if (pu->label != NULL)
        {
            /* #hogege is not a label but a filename */
            Str tmp2 = Strnew("#");
            tmp2->Push(pu->label);
            pu->file = tmp2->ptr;
            pu->real_file = cleanupName(file_unquote(pu->file));
            pu->label = NULL;
        }
        else
        {
            /* given URL must be null string */
#ifdef SOCK_DEBUG
            sock_log("given URL must be null string\n");
#endif
            return;
        }
    }

    this->scheme = pu->scheme;
    this->url = parsedURL2Str(pu)->ptr;
    pu->is_nocache = (option->flag & RG_NOCACHE);
    this->ext = filename_extension(pu->file, 1);

    hr->command = HR_COMMAND_GET;
    hr->flag = 0;
    hr->referer = option->referer;
    hr->request = request;

    switch (pu->scheme)
    {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
        if (request && request->body)
            /* local CGI: POST */
            this->stream = newFileStream(localcgi_post(pu->real_file, pu->query,
                                                       request, option->referer),
                                         (FileStreamCloseFunc)fclose);
        else
            /* lodal CGI: GET */
            this->stream = newFileStream(localcgi_get(pu->real_file, pu->query,
                                                      option->referer),
                                         (FileStreamCloseFunc)fclose);
        if (this->stream)
        {
            this->is_cgi = TRUE;
            this->scheme = pu->scheme = SCM_LOCAL_CGI;
            return;
        }
        examineFile(pu->real_file, this);
        if (this->stream == NULL)
        {
            if (dir_exist(pu->real_file))
            {
                add_index_file(pu, this);
                if (this->stream == NULL)
                    return;
            }
            else if (document_root != NULL)
            {
                tmp = Strnew(document_root);
                if (tmp->Back() != '/' && pu->file[0] != '/')
                    tmp->Push('/');
                tmp->Push(pu->file);
                p = cleanupName(tmp->ptr);
                q = cleanupName(file_unquote(p));
                if (dir_exist(q))
                {
                    pu->file = p;
                    pu->real_file = q;
                    add_index_file(pu, this);
                    if (this->stream == NULL)
                    {
                        return;
                    }
                }
                else
                {
                    examineFile(q, this);
                    if (this->stream)
                    {
                        pu->file = p;
                        pu->real_file = q;
                    }
                }
            }
        }
        if (this->stream == NULL && retryAsHttp && url[0] != '/')
        {
            if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN)
            {
                /* retry it as "http://" */
                u = Strnew_m_charp("http://", url, NULL)->ptr;
                goto retry;
            }
        }
        return;
    case SCM_FTP:
    case SCM_FTPDIR:
        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (non_null(FTP_proxy) &&
            !Do_not_use_proxy &&
            pu->host != NULL && !check_no_proxy(pu->host))
        {
            hr->flag |= HR_FLAG_PROXY;
            sock = openSocket(FTP_proxy_parsed.host,
                              GetScheme(FTP_proxy_parsed.scheme).name.data(),
                              FTP_proxy_parsed.port);
            if (sock < 0)
                return;
            this->scheme = SCM_HTTP;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            write(sock, tmp->ptr, tmp->Size());
        }
        else
        {
            this->stream = openFTPStream(pu, this);
            this->scheme = pu->scheme;
            return;
        }
        break;
    case SCM_HTTP:

    case SCM_HTTPS:

        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (request && request->method == FORM_METHOD_POST && request->body)
            hr->command = HR_COMMAND_POST;
        if (request && request->method == FORM_METHOD_HEAD)
            hr->command = HR_COMMAND_HEAD;
        if ((

                (pu->scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) :

                                          non_null(HTTP_proxy)) &&
            !Do_not_use_proxy &&
            pu->host != NULL && !check_no_proxy(pu->host))
        {
            hr->flag |= HR_FLAG_PROXY;
            if (pu->scheme == SCM_HTTPS && *status == HTST_CONNECT)
            {
                // https proxy の時に通る？
                assert(false);
                return;
                // sock = ssl_socket_of(ouf->stream);
                // if (!(sslh = openSSLHandle(sock, pu->host,
                //                            &this->ssl_certificate)))
                // {
                //     *status = HTST_MISSING;
                //     return;
                // }
            }
            else if (pu->scheme == SCM_HTTPS)
            {
                sock = openSocket(HTTPS_proxy_parsed.host,
                                  GetScheme(HTTPS_proxy_parsed.scheme).name.data(), HTTPS_proxy_parsed.port);
                sslh = NULL;
            }
            else
            {
                sock = openSocket(HTTP_proxy_parsed.host,
                                  GetScheme(HTTP_proxy_parsed.scheme).name.data(), HTTP_proxy_parsed.port);
                sslh = NULL;
            }

            if (sock < 0)
            {
#ifdef SOCK_DEBUG
                sock_log("Can't open socket\n");
#endif
                return;
            }
            if (pu->scheme == SCM_HTTPS)
            {
                if (*status == HTST_NORMAL)
                {
                    hr->command = HR_COMMAND_CONNECT;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_CONNECT;
                }
                else
                {
                    hr->flag |= HR_FLAG_LOCAL;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_NORMAL;
                }
            }
            else
            {
                tmp = HTTPrequest(pu, current, hr, extra_header);
                *status = HTST_NORMAL;
            }
        }
        else
        {
            sock = openSocket(pu->host,
                              GetScheme(pu->scheme).name.data(), pu->port);
            if (sock < 0)
            {
                *status = HTST_MISSING;
                return;
            }
            if (pu->scheme == SCM_HTTPS)
            {
                if (!(sslh = openSSLHandle(sock, pu->host,
                                           &this->ssl_certificate)))
                {
                    *status = HTST_MISSING;
                    return;
                }
            }
            hr->flag |= HR_FLAG_LOCAL;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            *status = HTST_NORMAL;
        }
        if (pu->scheme == SCM_HTTPS)
        {
            this->stream = newSSLStream(sslh, sock);
            if (sslh)
                SSL_write(sslh, tmp->ptr, tmp->Size());
            else
                write(sock, tmp->ptr, tmp->Size());
            if (w3m_reqlog)
            {
                FILE *ff = fopen(w3m_reqlog, "a");
                if (sslh)
                    fputs("HTTPS: request via SSL\n", ff);
                else
                    fputs("HTTPS: request without SSL\n", ff);
                fwrite(tmp->ptr, sizeof(char), tmp->Size(), ff);
                fclose(ff);
            }
            if (hr->command == HR_COMMAND_POST &&
                request->enctype == FORM_ENCTYPE_MULTIPART)
            {
                if (sslh)
                    SSL_write_from_file(sslh, request->body);
                else
                    write_from_file(sock, request->body);
            }
            return;
        }
        else
        {
            write(sock, tmp->ptr, tmp->Size());
            if (w3m_reqlog)
            {
                FILE *ff = fopen(w3m_reqlog, "a");
                fwrite(tmp->ptr, sizeof(char), tmp->Size(), ff);
                fclose(ff);
            }
            if (hr->command == HR_COMMAND_POST &&
                request->enctype == FORM_ENCTYPE_MULTIPART)
                write_from_file(sock, request->body);
        }
        break;
    case SCM_NNTP:
    case SCM_NNTP_GROUP:
    case SCM_NEWS:
    case SCM_NEWS_GROUP:
        if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NEWS)
            this->scheme = SCM_NEWS;
        else
            this->scheme = SCM_NEWS_GROUP;
        this->stream = openNewsStream(pu);
        return;
    case SCM_DATA:
        if (pu->file == NULL)
            return;
        p = Strnew(pu->file)->ptr;
        q = strchr(p, ',');
        if (q == NULL)
            return;
        *q++ = '\0';
        tmp = Strnew(q);
        q = strrchr(p, ';');
        if (q != NULL && !strcmp(q, ";base64"))
        {
            *q = '\0';
            this->encoding = ENC_BASE64;
        }
        else
            tmp = tmp->UrlDecode(FALSE, FALSE);
        this->stream = newStrStream(tmp);
        this->guess_type = (*p != '\0') ? p : (char *)"text/plain";
        return;
    case SCM_UNKNOWN:
    default:
        return;
    }
    this->stream = newInputStream(sock);
    return;
}