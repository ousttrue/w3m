#include "Content.h"
#include "network.h"
#include "alloc.h"
#include "mimetypes.h"
#include "form.h"
#include "http.h"
#include "HttpClient.h"
#include "ssl_util.h"
#include "local.h"
#include "myctype.h"
#include "proxy.h"
#include "etc.h"
#include "auth.h"
#include "quote.h"
#include <openssl/ssl.h>
#include <unistd.h>
#include <zlib.h>
#include <assert.h>

static bool dir_exist(const char* path)
{
    if (path == NULL || *path == '\0')
        return 0;
    struct stat stbuf;
    if (stat(path, &stbuf) == -1)
        return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

static void add_index_file(struct Url* pu, struct URLFile* uf)
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

struct Content openLocal(const char* u, struct Url* current, FormList* post, const char* referer)
{
    // u = file_to_url(u);
    struct Url pu;
    parseURL2(u, &pu, current);

    if (pu.label != NULL) {
        // #hogege is not a label but a filename
        Str tmp2 = Strnew_charp("#");
        Strcat_charp(tmp2, pu.label);
        pu.file = tmp2->ptr;
        pu.real_file = cleanupName(file_unquote(pu.file));
        pu.label = NULL;
    }

    struct URLFile f;
    init_stream(&f, SCM_MISSING, NULL);
    if (post && post->body) {
        // local CGI: POST
        f.stream = newFileStream(localcgi_post(pu.real_file, pu.query, post, referer), &fclose);
    } else {
        // lodal CGI: GET
        f.stream = newFileStream(localcgi_get(pu.real_file, pu.query, referer), &fclose);
    }

    if (f.stream) {
        f.is_cgi = true;
        f.scheme = pu.scheme = SCM_LOCAL_CGI;
    } else {
        examineFile(&f, pu.real_file);
        if (f.stream == NULL) {
            if (dir_exist(pu.real_file)) {
                add_index_file(&pu, &f);
                // if (f.stream == NULL) {
                //     // return;
                // }
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
                    add_index_file(&pu, &f);
                    // if (f.stream == NULL) {
                    //     // return;
                    // }
                } else {
                    examineFile(&f, q);
                    if (f.stream) {
                        pu.file = p;
                        pu.real_file = q;
                    }
                }
            }
        }
    }
    if (!f.stream) {
        return (struct Content) {
            .pu = pu,
            .page = NULL,
        };
    }
    Str page = readAll(&f);

    const char* content_type = guessContentType(pu.file);
    if (content_type == NULL) {
        content_type = "text/plain";
    }
    // real_type = c.content_type;
    if (f.guess_type) {
        content_type = f.guess_type;
    }

    // term_raw();
    return (struct Content) {
        .pu = pu,
        .page = page,
        .charset = WC_CES_UTF_8,
        .real_type = content_type,
        .document_header = NULL,
    };

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
    // if (c.f.is_cgi) {
    //     /* local CGI */
    //     // searchHeader = true;
    // }
    // break;
}

bool is_gzip(const unsigned char* src)
{
    return (int)src[0] == 0x1f && (int)src[1] == 0x8b && (int)src[2] == 0x08;
}

static Str decode_gzip(unsigned char* src, int size)
{
    // auto sig = src.subspan(0, 3);
    if (!is_gzip(src)) {
        //     assert(false);
        //     uint8_t _debug[] = { sig[0], sig[1], sig[2] };
        return NULL;
    }

    z_stream strm = { 0 };
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    int ret = inflateInit2(&strm, 47);
    if (ret != Z_OK) {
        return NULL;
    }

    Str buffer = Strnew();

    strm.avail_in = size;
    strm.next_in = src;

    do {
        const int CHUNK = 16384;
        unsigned char out[CHUNK];
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        assert(ret != Z_STREAM_ERROR); /* state not clobbered */
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR; /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void)inflateEnd(&strm);
            assert(false);
            buffer = 0;
            break;
        }
        auto have = CHUNK - strm.avail_out;

        // auto before = buffer.size();
        // buffer.resize(before + have);
        // memcpy(buffer.data() + before, out, have);
        Strcat_charp_n(buffer, (const char*)out, have);
    } while (strm.avail_out == 0);

    inflateEnd(&strm);

    return buffer;
}

struct Content openHttp(struct HttpClient* c, const char* path, struct Url* current, FormList* post, const char* referer, bool no_cache)
{
    struct Url pu;
    parseURL2(path, &pu, current);
    pu.is_nocache = no_cache;
    if (LocalhostOnly && pu.host && !is_localhost(pu.host)) {
        pu.host = NULL;
    }
    if (pu.file == NULL) {
        pu.file = allocStr("/", -1);
    }

    struct HttpRequest hr = {
        .command = HR_COMMAND_GET,
        .flag = 0,
        .referer = referer,
        .request = post,
    };
    if (post && post->method == FORM_METHOD_POST && post->body)
        hr.command = HR_COMMAND_POST;
    if (post && post->method == FORM_METHOD_HEAD)
        hr.command = HR_COMMAND_HEAD;

    //
    // open socket
    //
    Str tmp = NULL;
    SSL* sslh = NULL;
    int sock;
    TextList* extra_header = newTextList();
    if (((pu.scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) : non_null(HTTP_proxy))
        && use_proxy && pu.host != NULL && !check_no_proxy(pu.host)) {
        //
        // use proxy
        //
        hr.flag |= HR_FLAG_PROXY;
        if (pu.scheme == SCM_HTTPS && c->status == HTST_CONNECT) {
            sock = ssl_socket_of(c->f.stream);
            if (!(sslh = openSSLHandle(sock, pu.host,
                      &c->f.ssl_certificate))) {
                c->status = HTST_MISSING;
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
            if (c->status == HTST_NORMAL) {
                hr.command = HR_COMMAND_CONNECT;
                tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
                c->status = HTST_CONNECT;
            } else {
                hr.flag |= HR_FLAG_LOCAL;
                tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
                c->status = HTST_NORMAL;
            }
        } else {
            tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
            c->status = HTST_NORMAL;
        }
    } else {
        sock = openSocket(pu.host, getSchemeInfo(pu.scheme).name, pu.port);
        if (sock < 0) {
            c->status = HTST_MISSING;
            // return;
        }
        if (pu.scheme == SCM_HTTPS) {
            if (!(sslh = openSSLHandle(sock, pu.host,
                      &c->f.ssl_certificate))) {
                c->status = HTST_MISSING;
                // return;
            }
        }
        hr.flag |= HR_FLAG_LOCAL;
        tmp = getHttpRequestStr(&pu, current, &hr, extra_header);
        c->status = HTST_NORMAL;
    }

    init_stream(&c->f, SCM_MISSING, NULL);
    if (pu.scheme == SCM_HTTPS) {
        c->f = (struct URLFile) {
            .scheme = pu.scheme,
            .url = parsedURL2Str(&pu)->ptr,
            .ext = filename_extension(pu.file, 1),
            .stream = newSSLStream(sslh, sock),
        };
        // if (sslh)
        SSL_write(sslh, tmp->ptr, tmp->length);
        // else
        //     write(sock, tmp->ptr, tmp->length);

        if (hr.command == HR_COMMAND_POST && post->enctype == FORM_ENCTYPE_MULTIPART) {
            // if (sslh) {
            SSL_write_from_file(sslh, post->body);
            // } else {
            //     write_from_file(sock, post->body);
            // }
        }
    } else {
        c->f = (struct URLFile) {
            .scheme = pu.scheme,
            .url = parsedURL2Str(&pu)->ptr,
            .ext = filename_extension(pu.file, 1),
            .stream = newInputStream(sock),
        };
        write(sock, tmp->ptr, tmp->length);
        if (hr.command == HR_COMMAND_POST && post->enctype == FORM_ENCTYPE_MULTIPART)
            write_from_file(sock, post->body);
    }

    // term_cbreak();
    // message(getUI(), MSG_INFO, Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr);
    // refresh(ttyWriter());

    struct HttpResponse response = readHttpResponse(&c->f, &pu);
    const char* p;
    if (((response.status_code >= 301 && response.status_code <= 303)
            || response.status_code == 307)
        && (p = (char*)getHttpHeaderValue(response.headers, "Location:")) != NULL) {
        /* document moved */
        /* 301: Moved Permanently */
        /* 302: Found */
        /* 303: See Other */
        /* 307: Temporary Redirect (HTTP/1.1) */
        // c->url = url_encode(p, NULL, 0);
        post = NULL;
        UFclose(&c->f);
        current = New(struct Url);
        copyParsedURL(current, &pu);
        // t_buf->bufferprop |= BP_REDIRECTED;
        c->status = HTST_NORMAL;

        // redirect
        // && checkRedirection(&c, &pu)
        // return openHttp();
        abort();
    }

    //
    // read body
    //
    struct ContentTypeCharset cc = getContentType(response.headers);
    c->content_type = cc.content_type;
    if (c->content_type == NULL && pu.file != NULL) {
        if (!((response.status_code >= 400 && response.status_code <= 407) || (response.status_code >= 500 && response.status_code <= 505))) {
            c->content_type = guessContentType(pu.file);
        }
    }
    if (c->content_type == NULL)
        c->content_type = "text/plain";
    if (c->add_auth_cookie_flag && c->realm && c->uname && c->pwd) {
        /* If authorization is required and passed */
        add_auth_user_passwd(&pu, qstr_unquote(c->realm)->ptr, c->uname, c->pwd,
            0);
        c->add_auth_cookie_flag = 0;
    }
    if ((p = getHttpHeaderValue(response.headers, "WWW-Authenticate:")) != NULL && response.status_code == 401) {
        /* Authentication needed */
        struct http_auth hauth;
        if (findAuthentication(&hauth, response.headers, "WWW-Authenticate:") != NULL
            && (c->realm = get_auth_param(hauth.param, "realm")) != NULL) {
            struct Url* auth_pu;
            //         auth_pu = &pu;
            //         getAuthCookie(&hauth, "Authorization:", extra_header,
            //             auth_pu, &hr, post, &uname, &pwd);
            //         if (uname == NULL) {
            //             /* abort */
            //             term_raw();
            //             return (struct Content) {
            //                 pu, c->f, c->page, c->charset, c->content_type, response.headers
            //             };
            //         }
            //         UFclose(&c->f);
            //         add_auth_cookie_flag = 1;
            //         c->status = HTST_NORMAL;
            //         continue;
            abort();
        }
    }
    if ((p = getHttpHeaderValue(response.headers, "Proxy-Authenticate:")) != NULL && response.status_code == 407) {
        //     /* Authentication needed */
        struct http_auth hauth;
        if (findAuthentication(&hauth, response.headers, "Proxy-Authenticate:")
                != NULL
            && (c->realm = get_auth_param(hauth.param, "realm")) != NULL) {
            //         auth_pu = schemeToProxy(pu.scheme);
            //         getAuthCookie(&hauth, "Proxy-Authorization:",
            //             extra_header, auth_pu, &hr, post,
            //             &uname, &pwd);
            //         if (uname == NULL) {
            //             /* abort */
            //             term_raw();
            //             return (struct Content) {
            //                 pu, c->f, c->page, c->charset, c->content_type, response.headers
            //             };
            //         }
            //         UFclose(&c->f);
            //         add_auth_cookie_flag = 1;
            //         c->status = HTST_NORMAL;
            //         add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
            //         continue;
            abort();
        }
    }

    if (c->status == HTST_CONNECT) {
        /* XXX: RFC2617 3.2.3 Authentication-Info: ? */
        abort();
    }

    c->f.modtime = mymktime(getHttpHeaderValue(response.headers, "Last-Modified:"));

    /* XXX: can we use guess_type to give the type to loadHTMLstream
     *      to support default utf8 encoding for XHTML here? */
    c->f.guess_type = c->content_type;

    // Buffer* t_buf = newBuffer();
    if ((c->f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        uncompress_stream(&c->f, &pu.real_file);
    } else if (c->f.compression != CMP_NOCOMPRESS) {
        if (is_text_type(c->content_type)) {
            // uncompress_stream(&c->f, &t_buf->sourcefile);
            // uncompressed_file_type(c->pu.file, &c->f.ext);
            Str src = readAll(&c->f);
            c->page = decode_gzip((unsigned char*)src->ptr, src->length);
        } else {
            c->content_type = compress_application_type(c->f.compression);
            c->f.compression = CMP_NOCOMPRESS;
        }
    }

    // term_raw();
    return (struct Content) {
        pu, c->page, c->charset, c->content_type, NULL
    };
    // if (c->status == HTST_MISSING) {
    //     term_raw();
    //     UFclose(&c->f);
    //     return (struct Content) {};
    // }
}

struct Content
loadGeneralFile(const char* path, struct Url* current, FormList* post, const char* referer,
    bool no_cache)
{
    //         openURL(&c, &pu, current, post, referer, no_cache, extra_header, &hr);
    // void openURL(struct HttpClient* c, struct Url* pu, struct Url* current,
    //     FormList* post, const char* referer, bool no_cache, TextList* extra_header,
    //     struct HttpRequest* hr)

    struct Url pu;
    parseURL2(path, &pu, current);

    // enum UrlScheme scheme = getUrlScheme(c.url);
    // const char* u = (current == NULL && getUrlScheme(c.url) == SCM_MISSING && !ArgvIsURL)
    //     ? file_to_url(c.url) /* force to local file */
    //     : c.url;

    switch (pu.scheme) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI: {
        struct Content content = openLocal(path, current, post, referer);
        if (content.page) {
            return content;
        } else if (retryAsHttp) {
            //     // if (c.f.stream == NULL && retryAsHttp && c.url[0] != '/') {
            // const char* tmp = path;
            // enum UrlScheme scheme = getUrlScheme(path);
            //     //     if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
            // retry it as "http://"
            // c.url = ;
            //     //         // continue;
            struct HttpClient c;
            initHttpClient(&c);
            return openHttp(&c, Strnew_m_charp("http://", path, NULL)->ptr, current, post, referer, no_cache);
            //     //     }
        }
    }

    case SCM_HTTP:
    case SCM_HTTPS: {
        struct HttpClient c;
        initHttpClient(&c);
        return openHttp(&c, path, current, post, referer, no_cache);
    }

    default:
        return (struct Content) {};
    }
}
