#include "Content.h"
#include "form.h"
#include "http.h"
#include "HttpClient.h"
#include "ssl_util.h"
#include "tty.h"
#include "local.h"
#include "myctype.h"
#include "indep.h"
#include "proxy.h"
#include "ui.h"
#include "etc.h"
#include "auth.h"
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

struct Content
loadGeneralFile(const char* path, ParsedURL* current, FormList* post, const char* referer,
    bool no_cache)
{
    ParsedURL pu;
    // MySignalHandler (*prevtrap)(int _dummy) = NULL;
    TextList* extra_header = newTextList();
    Str uname = NULL;
    Str pwd = NULL;
    Str realm = NULL;
    bool add_auth_cookie_flag = 0;
    struct HttpRequest hr;
    ParsedURL* auth_pu;

    struct HttpClient c;
    initHttpClient(&c, path);

    // redirection loop
    while (true) {
        parseURL2(c.url, &pu, current);
        // const char* sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
        // if (sc_redirect && *sc_redirect && checkRedirection(&c, &pu)) {
        //     c.url = sc_redirect;
        //     post = NULL;
        //     add_auth_cookie_flag = 0;
        //     current = New(ParsedURL);
        //     *current = pu;
        //     c.status = HTST_NORMAL;
        //     continue;
        // }

        term_raw();
        //         openURL(&c, &pu, current, post, referer, no_cache, extra_header, &hr);
        // void openURL(struct HttpClient* c, ParsedURL* pu, ParsedURL* current,
        //     FormList* post, const char* referer, bool no_cache, TextList* extra_header,
        //     struct HttpRequest* hr)
        {
            init_stream(&c.f, SCM_MISSING, NULL);

            const char* u = (current == NULL && getUrlScheme(c.url) == SCM_MISSING && !ArgvIsURL)
                ? file_to_url(c.url) /* force to local file */
                : c.url;

            parseURL2(u, &pu, current);
            if (pu.scheme == SCM_LOCAL && pu.file == NULL) {
                if (pu.label != NULL) {
                    /* #hogege is not a label but a filename */
                    Str tmp2 = Strnew_charp("#");
                    Strcat_charp(tmp2, pu.label);
                    pu.file = tmp2->ptr;
                    pu.real_file = cleanupName(file_unquote(pu.file));
                    pu.label = NULL;
                } else {
                    /* given URL must be null string */
                    // return;
                }
            } else {

                if (LocalhostOnly && pu.host && !is_localhost(pu.host))
                    pu.host = NULL;

                c.f.scheme = pu.scheme;
                c.f.url = parsedURL2Str(&pu)->ptr;
                pu.is_nocache = no_cache;
                c.f.ext = filename_extension(pu.file, 1);

                hr.command = HR_COMMAND_GET;
                hr.flag = 0;
                hr.referer = referer;
                hr.request = post;

                switch (pu.scheme) {
                case SCM_LOCAL:
                case SCM_LOCAL_CGI: {
                    if (post && post->body)
                        /* local CGI: POST */
                        c.f.stream = newFileStream(localcgi_post(pu.real_file, pu.query,
                                                       post, referer),
                            (void (*)())fclose);
                    else
                        /* lodal CGI: GET */
                        c.f.stream = newFileStream(localcgi_get(pu.real_file, pu.query,
                                                       referer),
                            (void (*)())fclose);
                    if (c.f.stream) {
                        c.f.is_cgi = true;
                        c.f.scheme = pu.scheme = SCM_LOCAL_CGI;
                        // return;
                    } else {
                        examineFile(&c.f, pu.real_file);
                        if (c.f.stream == NULL) {
                            if (dir_exist(pu.real_file)) {
                                add_index_file(&pu, &c.f);
                                if (c.f.stream == NULL) {
                                    // return;
                                }
                            } else if (document_root != NULL) {
                                Str tmp = Strnew_charp(document_root);
                                if (Strlastchar(tmp) != '/' && pu.file[0] != '/')
                                    Strcat_char(tmp, '/');
                                Strcat_charp(tmp, pu.file);
                                char* p = cleanupName(tmp->ptr);
                                char* q = cleanupName(file_unquote(p));
                                if (dir_exist(q)) {
                                    pu.file = p;
                                    pu.real_file = q;
                                    add_index_file(&pu, &c.f);
                                    if (c.f.stream == NULL) {
                                        // return;
                                    }
                                } else {
                                    examineFile(&c.f, q);
                                    if (c.f.stream) {
                                        pu.file = p;
                                        pu.real_file = q;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    break;
                }
                case SCM_HTTP:
                case SCM_HTTPS: {
                    if (pu.file == NULL)
                        pu.file = allocStr("/", -1);
                    if (post && post->method == FORM_METHOD_POST && post->body)
                        hr.command = HR_COMMAND_POST;
                    if (post && post->method == FORM_METHOD_HEAD)
                        hr.command = HR_COMMAND_HEAD;

                    Str tmp = NULL;
                    SSL* sslh = NULL;
                    int sock;
                    if ((
                            (pu.scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) : non_null(HTTP_proxy))
                        && use_proxy && pu.host != NULL && !check_no_proxy(pu.host)) {
                        hr.flag |= HR_FLAG_PROXY;
                        if (pu.scheme == SCM_HTTPS && c.status == HTST_CONNECT) {
                            sock = ssl_socket_of(c.f.stream);
                            if (!(sslh = openSSLHandle(sock, pu.host,
                                      &c.f.ssl_certificate))) {
                                c.status = HTST_MISSING;
                                // return;
                            }
                        } else if (pu.scheme == SCM_HTTPS) {
                            sock = openSocket(HTTPS_proxy_parsed.host,
                                getSchemeInfo(HTTPS_proxy_parsed.scheme).name,
                                HTTPS_proxy_parsed.port);
                            sslh = NULL;
                        } else {
                            sock = openSocket(HTTP_proxy_parsed.host,
                                getSchemeInfo(HTTP_proxy_parsed.scheme).name,
                                HTTP_proxy_parsed.port);
                            sslh = NULL;
                        }
                        if (sock < 0) {
                            // return;
                        }
                        if (pu.scheme == SCM_HTTPS) {
                            if (c.status == HTST_NORMAL) {
                                hr.command = HR_COMMAND_CONNECT;
                                tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
                                c.status = HTST_CONNECT;
                            } else {
                                hr.flag |= HR_FLAG_LOCAL;
                                tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
                                c.status = HTST_NORMAL;
                            }
                        } else {
                            tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
                            c.status = HTST_NORMAL;
                        }
                    } else {
                        sock = openSocket(pu.host, getSchemeInfo(pu.scheme).name, pu.port);
                        if (sock < 0) {
                            c.status = HTST_MISSING;
                            // return;
                        }
                        if (pu.scheme == SCM_HTTPS) {
                            if (!(sslh = openSSLHandle(sock, pu.host,
                                      &c.f.ssl_certificate))) {
                                c.status = HTST_MISSING;
                                // return;
                            }
                        }
                        hr.flag |= HR_FLAG_LOCAL;
                        tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
                        c.status = HTST_NORMAL;
                    }
                    if (pu.scheme == SCM_HTTPS) {
                        c.f.stream = newSSLStream(sslh, sock);
                        if (sslh)
                            SSL_write(sslh, tmp->ptr, tmp->length);
                        else
                            write(sock, tmp->ptr, tmp->length);
                        // if (w3m_reqlog) {
                        //     FILE* ff = fopen(w3m_reqlog, "a");
                        //     if (ff == NULL)
                        //         // return;
                        //     if (sslh)
                        //         fputs("HTTPS: request via SSL\n", ff);
                        //     else
                        //         fputs("HTTPS: request without SSL\n", ff);
                        //     fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                        //     fclose(ff);
                        // }
                        if (hr.command == HR_COMMAND_POST && post->enctype == FORM_ENCTYPE_MULTIPART) {
                            if (sslh) {
                                SSL_write_from_file(sslh, post->body);
                            } else {
                                write_from_file(sock, post->body);
                            }
                        }
                        // return;
                    } else {
                        write(sock, tmp->ptr, tmp->length);
                        // if (w3m_reqlog) {
                        //     FILE* ff = fopen(w3m_reqlog, "a");
                        //     if (ff) {
                        //         fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                        //         fclose(ff);
                        //     }
                        // }
                        if (hr.command == HR_COMMAND_POST && post->enctype == FORM_ENCTYPE_MULTIPART)
                            write_from_file(sock, post->body);
                    }
                    c.f.stream = newInputStream(sock);
                    break;
                }
                // case SCM_DATA: {
                //     if (pu.file == NULL)
                //         return;
                //     char* p = Strnew_charp(pu.file)->ptr;
                //     char* q = strchr(p, ',');
                //     if (q == NULL)
                //         return;
                //     *q++ = '\0';
                //     Str tmp = Strnew_charp(q);
                //     q = strrchr(p, ';');
                //     if (q != NULL && !strcmp(q, ";base64")) {
                //         *q = '\0';
                //         c.f.encoding = ENC_BASE64;
                //     } else
                //         tmp = Str_url_unquote(tmp, FALSE, FALSE);
                //     c.f.stream = newStrStream(tmp);
                //     c.f.guess_type = (*p != '\0') ? p : "text/plain";
                //     return;
                // }
                case SCM_UNKNOWN:
                default:
                    // return;
                    break;
                }
            }
            if (c.f.stream == NULL && retryAsHttp && c.url[0] != '/') {
                const char* tmp = c.url;
                enum UrlScheme scheme = parseUrlScheme(&tmp);
                if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
                    /* retry it as "http://" */
                    c.url = Strnew_m_charp("http://", tmp, NULL)->ptr;
                    continue;
                }
            }

            // content_charset = 0;
            // if (!c.f.stream) {
            //     switch (c.f.scheme) {
            //     case SCM_LOCAL: {
            //         struct stat st;
            //         if (stat(pu.real_file, &st) < 0)
            //             return NULL;
            //         if (S_ISDIR(st.st_mode)) {
            //             if (UseExternalDirBuffer) {
            //                 Str cmd = Sprintf("%s?dir=%s#current",
            //                     DirBufferCommand, pu.file);
            //                 Buffer* b = loadGeneralFile(cmd->ptr, NULL, NULL, NO_REFERER, 0,
            //                     do_download);
            //                 if (b != NULL && b != NO_BUFFER) {
            //                     copyParsedURL(&b->currentURL, &pu);
            //                     b->filename = b->currentURL.real_file;
            //                 }
            //                 return b;
            //             } else {
            //                 c.page = loadLocalDir(pu.real_file);
            //                 c.content_type = "local:directory";
            //                 c.charset = SystemCharset;
            //             }
            //         }
            //     } break;
            //     case SCM_UNKNOWN:
            //         /* FIXME: gettextize? */
            //         message(getUI(), MSG_ERR, Sprintf("Unknown URI: %s", parsedURL2Str(&pu)->ptr)->ptr);
            //         break;
            //
            //     default:
            //         break;
            //     }
            //     if (c.page && c.page->length > 0) {
            //         term_raw();
            //         return page_loaded(pu, c.f, c.page, c.charset, c.content_type, NULL, do_download);
            //     }
            //     return NULL;
            // }

            if (c.status == HTST_MISSING) {
                term_raw();
                UFclose(&c.f);
                return (struct Content) {};
            }

            /* openURL() succeeded */
            // if (sigsetjmp(AbortLoading, 1) != 0) {
            //     /* transfer interrupted */
            //     term_raw();
            //     if (b)
            //         discardBuffer(b);
            //     UFclose(&f);
            //     return NULL;
            // }

            // Buffer *b = NULL;
            if (c.f.is_cgi) {
                /* local CGI */
                // searchHeader = true;
            }
            // if (header_string)
            //     header_string = NULL;
            // TRAP_ON;
            if (pu.scheme == SCM_HTTP || pu.scheme == SCM_HTTPS) {
                term_cbreak();
                message(getUI(), MSG_INFO, Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr);
                // refresh(ttyWriter());

                struct HttpResponse response = readHttpResponse(&c.f, &pu);
                const char* p;
                if (((response.status_code >= 301 && response.status_code <= 303)
                        || response.status_code == 307)
                    && (p = (char*)getHttpHeaderValue(response.headers, "Location:")) != NULL
                    && checkRedirection(&c, &pu)) {
                    /* document moved */
                    /* 301: Moved Permanently */
                    /* 302: Found */
                    /* 303: See Other */
                    /* 307: Temporary Redirect (HTTP/1.1) */
                    c.url = url_encode(p, NULL, 0);
                    post = NULL;
                    UFclose(&c.f);
                    current = New(ParsedURL);
                    copyParsedURL(current, &pu);
                    // t_buf->bufferprop |= BP_REDIRECTED;
                    c.status = HTST_NORMAL;
                    continue;
                }
                struct ContentTypeCharset cc = getContentType(response.headers);
                c.content_type = cc.content_type;
                if (c.content_type == NULL && pu.file != NULL) {
                    if (!((response.status_code >= 400 && response.status_code <= 407) || (response.status_code >= 500 && response.status_code <= 505))) {
                        c.content_type = guessContentType(pu.file);
                    }
                }
                if (c.content_type == NULL)
                    c.content_type = "text/plain";
                if (add_auth_cookie_flag && realm && uname && pwd) {
                    /* If authorization is required and passed */
                    add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd,
                        0);
                    add_auth_cookie_flag = 0;
                }
                if ((p = getHttpHeaderValue(response.headers, "WWW-Authenticate:")) != NULL && response.status_code == 401) {
                    /* Authentication needed */
                    struct http_auth hauth;
                    if (findAuthentication(&hauth, response.headers, "WWW-Authenticate:") != NULL
                        && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                        auth_pu = &pu;
                        getAuthCookie(&hauth, "Authorization:", extra_header,
                            auth_pu, &hr, post, &uname, &pwd);
                        if (uname == NULL) {
                            /* abort */
                            term_raw();
                            return (struct Content) {
                                pu, c.f, c.page, c.charset, c.content_type, response.headers
                            };
                        }
                        UFclose(&c.f);
                        add_auth_cookie_flag = 1;
                        c.status = HTST_NORMAL;
                        continue;
                    }
                }
                if ((p = getHttpHeaderValue(response.headers, "Proxy-Authenticate:")) != NULL && response.status_code == 407) {
                    /* Authentication needed */
                    struct http_auth hauth;
                    if (findAuthentication(&hauth, response.headers, "Proxy-Authenticate:")
                            != NULL
                        && (realm = get_auth_param(hauth.param, "realm")) != NULL) {
                        auth_pu = schemeToProxy(pu.scheme);
                        getAuthCookie(&hauth, "Proxy-Authorization:",
                            extra_header, auth_pu, &hr, post,
                            &uname, &pwd);
                        if (uname == NULL) {
                            /* abort */
                            term_raw();
                            return (struct Content) {
                                pu, c.f, c.page, c.charset, c.content_type, response.headers
                            };
                        }
                        UFclose(&c.f);
                        add_auth_cookie_flag = 1;
                        c.status = HTST_NORMAL;
                        add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
                        continue;
                    }
                }
                /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

                if (c.status == HTST_CONNECT) {
                    continue;
                }

                c.f.modtime = mymktime(getHttpHeaderValue(response.headers, "Last-Modified:"));
            } else {
                c.content_type = guessContentType(pu.file);
                if (c.content_type == NULL)
                    c.content_type = "text/plain";
                // real_type = c.content_type;
                if (c.f.guess_type) {
                    c.content_type = c.f.guess_type;
                }
            }
            break;
        }
    }

    /* XXX: can we use guess_type to give the type to loadHTMLstream
     *      to support default utf8 encoding for XHTML here? */
    c.f.guess_type = c.content_type;

    term_raw();
    return (struct Content) {
        pu, c.f, c.page, c.charset, c.content_type, NULL
    };
}
