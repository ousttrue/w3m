#include "HttpClient.h"
#include "url_scheme.h"
#include "http.h"
#include "form.h"
#include "local.h"
#include "proxy.h"
#include "istream.h"
#include "ssl_util.h"
#include "etc.h"
#include "indep.h"
#include <openssl/ssl.h>
#include <unistd.h>

static bool dir_exist(const char* path)
{
    if (path == NULL || *path == '\0')
        return 0;

    struct stat stbuf;
    if (stat(path, &stbuf) == -1)
        return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

static void add_index_file(ParsedURL* pu, struct URLFile* uf)
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
        examineFile(uf, q);
        if (uf->stream != NULL) {
            pu->file = p;
            pu->real_file = q;
            return;
        }
    }
}

static void write_from_file(int sock, const char* file)
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

void openURL(struct HttpClient* c, const char* url, ParsedURL* pu, ParsedURL* current,
    struct URLOption* option, FormList* request, TextList* extra_header,
    struct HttpRequest* hr)
{
    init_stream(&c->f, SCM_MISSING, NULL);

    const char* u = url;
    enum UrlScheme scheme = getURLScheme(&u);
    if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL)
        u = file_to_url(url); /* force to local file */
    else
        u = url;

    int sock;
    while (true) {
        parseURL2(u, pu, current);
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
                return;
            }
        }

        if (LocalhostOnly && pu->host && !is_localhost(pu->host))
            pu->host = NULL;

        c->f.scheme = pu->scheme;
        c->f.url = parsedURL2Str(pu)->ptr;
        pu->is_nocache = (option->flag & RG_NOCACHE);
        c->f.ext = filename_extension(pu->file, 1);

        hr->command = HR_COMMAND_GET;
        hr->flag = 0;
        hr->referer = option->referer;
        hr->request = request;

        switch (pu->scheme) {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (request && request->body)
                /* local CGI: POST */
                c->f.stream = newFileStream(localcgi_post(pu->real_file, pu->query,
                                                request, option->referer),
                    (void (*)())fclose);
            else
                /* lodal CGI: GET */
                c->f.stream = newFileStream(localcgi_get(pu->real_file, pu->query,
                                                option->referer),
                    (void (*)())fclose);
            if (c->f.stream) {
                c->f.is_cgi = TRUE;
                c->f.scheme = pu->scheme = SCM_LOCAL_CGI;
                return;
            }
            examineFile(&c->f, pu->real_file);
            if (c->f.stream == NULL) {
                if (dir_exist(pu->real_file)) {
                    add_index_file(pu, &c->f);
                    if (c->f.stream == NULL)
                        return;
                } else if (document_root != NULL) {
                    Str tmp = Strnew_charp(document_root);
                    if (Strlastchar(tmp) != '/' && pu->file[0] != '/')
                        Strcat_char(tmp, '/');
                    Strcat_charp(tmp, pu->file);
                    char* p = cleanupName(tmp->ptr);
                    char* q = cleanupName(file_unquote(p));
                    if (dir_exist(q)) {
                        pu->file = p;
                        pu->real_file = q;
                        add_index_file(pu, &c->f);
                        if (c->f.stream == NULL) {
                            return;
                        }
                    } else {
                        examineFile(&c->f, q);
                        if (c->f.stream) {
                            pu->file = p;
                            pu->real_file = q;
                        }
                    }
                }
            }
            if (c->f.stream == NULL && retryAsHttp && url[0] != '/') {
                if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
                    /* retry it as "http://" */
                    u = Strnew_m_charp("http://", url, NULL)->ptr;
                    continue;
                }
            }
            return;
        case SCM_HTTP:
        case SCM_HTTPS:
            if (pu->file == NULL)
                pu->file = allocStr("/", -1);
            if (request && request->method == FORM_METHOD_POST && request->body)
                hr->command = HR_COMMAND_POST;
            if (request && request->method == FORM_METHOD_HEAD)
                hr->command = HR_COMMAND_HEAD;

            Str tmp = NULL;
            SSL* sslh = NULL;
            if ((
                    (pu->scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) : non_null(HTTP_proxy))
                && use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
                hr->flag |= HR_FLAG_PROXY;
                if (pu->scheme == SCM_HTTPS && c->status == HTST_CONNECT) {
                    sock = ssl_socket_of(c->f.stream);
                    if (!(sslh = openSSLHandle(sock, pu->host,
                              &c->f.ssl_certificate))) {
                        c->status = HTST_MISSING;
                        return;
                    }
                } else if (pu->scheme == SCM_HTTPS) {
                    sock = openSocket(HTTPS_proxy_parsed.host,
                        schemeNumToName(HTTPS_proxy_parsed.scheme),
                        HTTPS_proxy_parsed.port);
                    sslh = NULL;
                } else {
                    sock = openSocket(HTTP_proxy_parsed.host,
                        schemeNumToName(HTTP_proxy_parsed.scheme),
                        HTTP_proxy_parsed.port);
                    sslh = NULL;
                }
                if (sock < 0) {
                    return;
                }
                if (pu->scheme == SCM_HTTPS) {
                    if (c->status == HTST_NORMAL) {
                        hr->command = HR_COMMAND_CONNECT;
                        tmp = getHttpRequestStr(pu, current, hr, extra_header);
                        c->status = HTST_CONNECT;
                    } else {
                        hr->flag |= HR_FLAG_LOCAL;
                        tmp = getHttpRequestStr(pu, current, hr, extra_header);
                        c->status = HTST_NORMAL;
                    }
                } else {
                    tmp = getHttpRequestStr(pu, current, hr, extra_header);
                    c->status = HTST_NORMAL;
                }
            } else {
                sock = openSocket(pu->host, schemeNumToName(pu->scheme), pu->port);
                if (sock < 0) {
                    c->status = HTST_MISSING;
                    return;
                }
                if (pu->scheme == SCM_HTTPS) {
                    if (!(sslh = openSSLHandle(sock, pu->host,
                              &c->f.ssl_certificate))) {
                        c->status = HTST_MISSING;
                        return;
                    }
                }
                hr->flag |= HR_FLAG_LOCAL;
                tmp = getHttpRequestStr(pu, current, hr, extra_header);
                c->status = HTST_NORMAL;
            }
            if (pu->scheme == SCM_HTTPS) {
                c->f.stream = newSSLStream(sslh, sock);
                if (sslh)
                    SSL_write(sslh, tmp->ptr, tmp->length);
                else
                    write(sock, tmp->ptr, tmp->length);
                if (w3m_reqlog) {
                    FILE* ff = fopen(w3m_reqlog, "a");
                    if (ff == NULL)
                        return;
                    if (sslh)
                        fputs("HTTPS: request via SSL\n", ff);
                    else
                        fputs("HTTPS: request without SSL\n", ff);
                    fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                    fclose(ff);
                }
                if (hr->command == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART) {
                    if (sslh)
                        SSL_write_from_file(sslh, request->body);
                    else
                        write_from_file(sock, request->body);
                }
                return;
            } else {
                write(sock, tmp->ptr, tmp->length);
                if (w3m_reqlog) {
                    FILE* ff = fopen(w3m_reqlog, "a");
                    if (ff == NULL)
                        return;
                    fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                    fclose(ff);
                }
                if (hr->command == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
                    write_from_file(sock, request->body);
            }
            break;
        case SCM_DATA: {
            if (pu->file == NULL)
                return;
            char* p = Strnew_charp(pu->file)->ptr;
            char* q = strchr(p, ',');
            if (q == NULL)
                return;
            *q++ = '\0';
            tmp = Strnew_charp(q);
            q = strrchr(p, ';');
            if (q != NULL && !strcmp(q, ";base64")) {
                *q = '\0';
                c->f.encoding = ENC_BASE64;
            } else
                tmp = Str_url_unquote(tmp, FALSE, FALSE);
            c->f.stream = newStrStream(tmp);
            c->f.guess_type = (*p != '\0') ? p : "text/plain";
            return;
        }
        case SCM_UNKNOWN:
        default:
            return;
        }
        break;
    }
    c->f.stream = newInputStream(sock);
}
