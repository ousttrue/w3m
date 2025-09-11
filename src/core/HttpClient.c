#include "HttpClient.h"
#include "quote.h"
#include "alloc.h"
#include "auth.h"
#include "html_form.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "http_message.h"
#include "cookie.h"
#include "Content.h"
#include "ContentType.h"
#include "myctype.h"
#include "network.h"
#include "proxy.h"
#include "istream.h"
#include "ssl_util.h"
#include "time_util.h"
#include "keymap.h"
#include <assert.h>
#include <openssl/ssl.h>
#include <unistd.h>
#include <zlib.h>

void httpInitClient(struct HttpClient* c, struct UserInteraction ui)
{
    memset(c, 0, sizeof(struct HttpClient));
    c->ui = ui;
    // c->status = HTST_NORMAL,
    // c->url = path;
    // c->page = NULL;
    // c->content_type = "text/plain";
    // c->charset = WC_CES_US_ASCII;
    // c->current_content_length = 0;
    c->uname = NULL;
    c->pwd = NULL;
    c->realm = NULL;
    c->add_auth_cookie_flag = 0;
    c->ssl_certificate = 0;
}

static void write_from_file(int sock, const char* file)
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

// connect request response
static bool request(struct HttpClient* c, int i,
    struct Url url, struct Url* current, struct Form* post, const char* referer)
{
    c->exchanges[i] = (struct HttpExchange) {
        .status = HTST_NORMAL,
        .stream = NULL,
        .request = { 0 },
        .response = { 0 },
    };

    //
    // HttpRequest
    //
    struct HttpRequest* req = &c->exchanges[i].request;
    *req = (struct HttpRequest) {
        .url = url,
        .method = HTTP_METHOD_GET,
        .flag = 0,
        .referer = referer,
        .post = post,
    };
    if (LocalhostOnly && req->url.host && !is_localhost(req->url.host)) {
        req->url.host = NULL;
    }
    if (!req->url.file) {
        req->url.file = allocStr("/", -1);
    }
    if (post) {
        if (post->method == FORM_METHOD_POST) {
            req->method = HTTP_METHOD_POST;
        } else if (post && post->method == FORM_METHOD_HEAD) {
            req->method = HTTP_METHOD_HEAD;
        }
    }

    //
    // open socket
    //
    Str tmp = NULL;
    SSL* sslh = NULL;
    int sock = -1;
    TextList* extra_header = newTextList();
    if (((req->url.scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) : non_null(HTTP_proxy))
        && use_proxy && req->url.host && !check_no_proxy(req->url.host)) {
        //
        // use proxy
        //
        abort();
        // req->flag |= HR_FLAG_PROXY;
        // if (req->url.scheme == SCM_HTTPS && exchange->status == HTST_CONNECT) {
        //     sock = c->f.stream->ssl.handle->sock;
        //     if (!(sslh = openSSLHandle(ui, sock, req->url.host, &c->f.ssl_certificate))) {
        //         c->status = HTST_MISSING;
        //         // return;
        //     }
        // } else if (req->url.scheme == SCM_HTTPS) {
        //     sock = openSocket(HTTPS_proxy_parsed.host,
        //         getSchemeInfo(HTTPS_proxy_parsed.scheme).name,
        //         HTTPS_proxy_parsed.port);
        //     sslh = NULL;
        // } else {
        //     sock = openSocket(HTTP_proxy_parsed.host,
        //         getSchemeInfo(HTTP_proxy_parsed.scheme).name,
        //         HTTP_proxy_parsed.port);
        //     sslh = NULL;
        // }
        // if (sock < 0) {
        //     // return;
        // }
        // if (hr.url.scheme == SCM_HTTPS) {
        //     if (c->status == HTST_NORMAL) {
        //         hr.method = HTTP_METHOD_CONNECT;
        //         tmp = getHttpRequestStr(&hr, current, extra_header);
        //         c->status = HTST_CONNECT;
        //     } else {
        //         hr.flag |= HR_FLAG_LOCAL;
        //         tmp = getHttpRequestStr(&hr, current, extra_header);
        //         c->status = HTST_NORMAL;
        //     }
        // } else {
        //     tmp = getHttpRequestStr(&hr, current, extra_header);
        //     c->status = HTST_NORMAL;
        // }
    } else {
        sock = openSocket(req->url.host, getSchemeInfo(req->url.scheme).name, req->url.port);
        if (sock < 0) {
            c->exchanges[i].status = HTST_MISSING;
            return false;
        }
        if (req->url.scheme == SCM_HTTPS) {
            sslh = openSSLHandle(c->ui, sock, req->url.host, &c->ssl_certificate);
            if (!sslh) {
                c->exchanges[i].status = HTST_MISSING;
                return false;
            }
        }
        // req->flag |= HR_FLAG_LOCAL;
        tmp = getHttpRequestStr(&c->exchanges[i].request, current, extra_header);
        c->exchanges[i].status = HTST_NORMAL;
    }

    //
    // send request
    //
    if (req->url.scheme == SCM_HTTPS) {
        c->exchanges[i].stream = newSSLStream(sslh, sock),
        SSL_write(sslh, tmp->ptr, tmp->length);
        if (req->method == HTTP_METHOD_POST && post->enctype == FORM_ENCTYPE_MULTIPART) {
            SSL_write_from_file(sslh, post->body);
        }
    } else {
        c->exchanges[i].stream = newInputStream(sock);
        write(sock, tmp->ptr, tmp->length);
        if (req->method == HTTP_METHOD_POST && post->enctype == FORM_ENCTYPE_MULTIPART) {
            write_from_file(sock, post->body);
        }
    }

    //
    // read response
    //
    c->exchanges[i].response = readHttpResponse(&req->url, c->exchanges[i].stream);
    return true;
}

struct Content httpRequest(struct HttpClient* c,
    const char* path, struct Url* current, struct Form* post, const char* referer)
{
    struct Url url = parseUrl(path, current);
    for (int i = 0; i < MAX_FOLLOW_REDIRECTION; ++i) {
        if (!request(c, i, url, current, post, referer)) {
            return (struct Content) {};
        }
        struct HttpRequest* req = &c->exchanges[i].request;
        struct HttpResponse* res = &c->exchanges[i].response;

        const char* p;
        if ((p = getHttpHeaderValue(res->headers, "content-transfer-encoding:"))) {
            abort();
            // if (!strncasecmp(p, "base64", 6))
            //     c->f.encoding = ENC_BASE64;
            // else if (!strncasecmp(p, "quoted-printable", 16))
            //     c->f.encoding = ENC_QUOTE;
            // else if (!strncasecmp(p, "uuencode", 8) || !strncasecmp(p, "x-uuencode", 10))
            //     c->f.encoding = ENC_UUENCODE;
            // else
            //     c->f.encoding = ENC_7BIT;
        }

        if (use_cookie
            && accept_cookie
            && check_cookie_accept_domain(c->exchanges[i].request.url.host)
            && ((p = getHttpHeaderValue(res->headers, "Set-Cookie:"))
                || (p = getHttpHeaderValue(res->headers, "Set-Cookie2:")))) {
            Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
                comment = NULL, commentURL = NULL, port = NULL;
            int version, quoted, flag = 0;
            time_t expires = (time_t)-1;

            Strremovetrailingspaces(name);
            if (*p == '=') {
                p++;
                SKIP_BLANKS(p);
                quoted = 0;
                const char* q = NULL;
                while (!IS_ENDL(*p) && (quoted || *p != ';')) {
                    if (!IS_SPACE(*p))
                        q = p;
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    Strcat_char(value, *(p++));
                }
                if (q)
                    Strshrink(value, p - q - 1);
            }
            while (*p == ';') {
                p++;
                SKIP_BLANKS(p);
                struct CharSlice tmp2;
                if (matchattr(p, (struct CharSlice) { "expires", 7 }, &tmp2)) {
                    /* version 0 */
                    expires = mymktime(Strnew_charp_n(tmp2.p, tmp2.len)->ptr);
                } else if (matchattr(p, (struct CharSlice) { "max-age", 7 }, &tmp2)) {
                    /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2 */
                    expires = time(NULL) + atol(Strnew_charp_n(tmp2.p, tmp2.len)->ptr);
                } else if (matchattr(p, (struct CharSlice) { "domain", 6 }, &tmp2)) {
                    domain = Strnew_charp_n(tmp2.p, tmp2.len);
                } else if (matchattr(p, (struct CharSlice) { "path", 4 }, &tmp2)) {
                    path = Strnew_charp_n(tmp2.p, tmp2.len);
                } else if (matchattr(p, (struct CharSlice) { "secure", 6 }, NULL)) {
                    flag |= COO_SECURE;
                } else if (matchattr(p, (struct CharSlice) { "comment", 7 }, &tmp2)) {
                    comment = Strnew_charp_n(tmp2.p, tmp2.len);
                } else if (matchattr(p, (struct CharSlice) { "version", 7 }, &tmp2)) {
                    version = atoi(Strnew_charp_n(tmp2.p, tmp2.len)->ptr);
                } else if (matchattr(p, (struct CharSlice) { "port", 4 }, &tmp2)) {
                    /* version 1, Set-Cookie2 */
                    port = Strnew_charp_n(tmp2.p, tmp2.len);
                } else if (matchattr(p, (struct CharSlice) { "commentURL", 10 }, &tmp2)) {
                    /* version 1, Set-Cookie2 */
                    commentURL = Strnew_charp_n(tmp2.p, tmp2.len);
                } else if (matchattr(p, (struct CharSlice) { "discard", 7 }, NULL)) {
                    /* version 1, Set-Cookie2 */
                    flag |= COO_DISCARD;
                }
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';')) {
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    p++;
                }
            }
            if (name->length > 0) {
                // if (show_cookie) {
                //     if (flag & COO_SECURE)
                //         message(getUI(), MSG_INFO, "Received a secured cookie");
                //     else
                //         message(getUI(), MSG_INFO, Sprintf("Received cookie: %s=%s", name->ptr, value->ptr)->ptr);
                // }
                int err = add_cookie(&c->exchanges[i].request.url, name, value, expires, domain, path, flag,
                    comment, version, port, commentURL);
                if (err) {
                    char* ans = (accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT)
                        ? "y"
                        : NULL;
                    // if ((err & COO_OVERRIDE_OK) && accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK) {
                    //     Str msg = Sprintf("Accept bad cookie from %s for %s?",
                    //         req->url.host,
                    //         ((domain && domain->ptr)
                    //                 ? domain->ptr
                    //                 : "<localdomain>"));
                    //     if (msg->length > getScreen()->COLS - 10)
                    //         Strshrink(msg, msg->length - (getScreen()->COLS - 10));
                    //     Strcat_charp(msg, " (y/n)");
                    //     ans = inputAnswer(msg->ptr);
                    // }
                    if (ans == NULL || TOLOWER(*ans) != 'y' || (err = add_cookie(&c->exchanges[i].request.url, name, value, expires, domain, path, flag | COO_OVERRIDE, comment, version, port, commentURL))) {
                        err = (err & ~COO_OVERRIDE_OK) - 1;
                        char* emsg;
                        if (err >= 0 && err < COO_EMAX)
                            emsg = Sprintf("This cookie was rejected "
                                           "to prevent security violation. [%s]",
                                getCookieViolationMsg(err))
                                       ->ptr;
                        else
                            emsg = "This cookie was rejected to prevent security violation.";
                        // if (show_cookie)
                        //     message(getUI(), MSG_ERR, emsg);
                    } else if (show_cookie) {
                        // message(getUI(), MSG_INFO, Sprintf("Accepting invalid cookie: %s=%s", name->ptr, value->ptr)->ptr);
                    }
                }
            }
        }

        if ((p = getHttpHeaderValue(res->headers, "w3m-control:"))
            && c->exchanges[i].request.url.scheme == SCM_LOCAL_CGI) {
            Str funcname = Strnew();
            SKIP_BLANKS(p);
            CommandFunc f = getFunc(funcname->ptr);
            Str tmp = Strnew_charp(p);
            Strchop(tmp);
            // TODO:
            // pushEvent(f, tmp->ptr);
        }

        if (((res->status_code >= 301 && res->status_code <= 303)
                || res->status_code == 307)
            && (p = getHttpHeaderValue(res->headers, "Location:")) != NULL) {
            // document moved
            // 301: Moved Permanently
            // 302: Found
            // 303: See Other
            // 307: Temporary Redirect (HTTP/1.1)
            ISclose(c->exchanges[i].stream);
            post = NULL;
            current = New(struct Url);
            *current = copyParsedUrl(&c->exchanges[i].request.url);
            // url = url_encode(p, NULL, 0);
            struct Url new_url = parseUrl(p, current);

            // check redirection loop
            for (int j = 0; j <= i; ++j) {
                if (same_url_p(&new_url, &c->exchanges[i].request.url)) {
                    Str tmp = Sprintf("Redirection loop detected (%s)", parsedURL2Str(&new_url)->ptr);
                    // message(getUI(), MSG_ERR, tmp->ptr);
                    return (struct Content) { 0 };
                }
            }

            // t_buf->bufferprop |= BP_REDIRECTED;
            url = new_url;
            continue;
        }

        if (c->add_auth_cookie_flag && c->realm && c->uname && c->pwd) {
            /* If authorization is required and passed */
            add_auth_user_passwd(&req->url, qstr_unquote(c->realm)->ptr, c->uname, c->pwd,
                0);
            c->add_auth_cookie_flag = 0;
        }
        if ((p = getHttpHeaderValue(res->headers, "WWW-Authenticate:")) != NULL && res->status_code == 401) {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, res->headers, "WWW-Authenticate:") != NULL
                && (c->realm = get_auth_param(hauth.param, "realm")) != NULL) {
                struct Url* auth_pu;
                //         auth_pu = &req->url;
                //         getAuthCookie(&hauth, "Authorization:", extra_header,
                //             auth_pu, &hr, post, &uname, &pwd);
                //         if (uname == NULL) {
                //             /* abort */
                //             term_raw();
                //             return (struct Content) {
                //                 req->url, c->f, c->page, c->charset, c->content_type, res->headers
                //             };
                //         }
                //         UFclose(&c->f);
                //         add_auth_cookie_flag = 1;
                //         c->status = HTST_NORMAL;
                //         continue;
                abort();
            }
        }
        if ((p = getHttpHeaderValue(res->headers, "Proxy-Authenticate:")) != NULL && res->status_code == 407) {
            //     /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, res->headers, "Proxy-Authenticate:")
                    != NULL
                && (c->realm = get_auth_param(hauth.param, "realm")) != NULL) {
                //         auth_pu = schemeToProxy(req->url.scheme);
                //         getAuthCookie(&hauth, "Proxy-Authorization:",
                //             extra_header, auth_pu, &hr, post,
                //             &uname, &pwd);
                //         if (uname == NULL) {
                //             /* abort */
                //             term_raw();
                //             return (struct Content) {
                //                 req->url, c->f, c->page, c->charset, c->content_type, res->headers
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

        // if (c->status == HTST_CONNECT) {
        //     /* XXX: RFC2617 3.2.3 Authentication-Info: ? */
        //     abort();
        // }

        //
        // read body
        //
        struct ContentTypeCharset cc = getContentType(res->headers);
        struct Content content = {
            .url = c->exchanges[i].request.url,
            .page = 0,
            .cc = {
                .content_type = cc.content_type,
                .charset = cc.charset,
            },
        };
        if (content.cc.content_type == CONTENTTYPE_UNKNOWN && req->url.file != NULL) {
            if (!((res->status_code >= 400 && res->status_code <= 407)
                    || (res->status_code >= 500 && res->status_code <= 505))) {
                content.cc.content_type = guessContentType(req->url.file);
            }
        }
        if (content.cc.content_type == CONTENTTYPE_UNKNOWN) {
            content.cc.content_type = CONTENTTYPE_TEXT_PLAIN;
        }

        // c->f.modtime = mymktime(getHttpHeaderValue(res->headers, "Last-Modified:"));

        // /* XXX: can we use guess_type to give the type to loadHTMLstream
        //  *      to support default utf8 encoding for XHTML here? */
        // c->f.guess_type = c->content_type;

        // Buffer* t_buf = newBuffer();

        Str src = readAll(c->exchanges[i].stream);
        ISclose(c->exchanges[i].stream);
        enum CompressionType content_encoding = CMP_NOCOMPRESS;
        if ((p = getHttpHeaderValue(res->headers, "content-encoding:"))) {
            content_encoding = get_compression(p);
        }
        if (content_encoding != CMP_NOCOMPRESS) {
            content.page = decode_gzip((unsigned char*)src->ptr, src->length);
        } else {
            content.page = src;
        }

        // term_raw();
        return content;
    }

    // fail redirection
    {
        Str tmp = Sprintf("Number of redirections exceeded %d", MAX_FOLLOW_REDIRECTION);
        // message(getUI(), MSG_ERR, tmp->ptr);
        return (struct Content) {};
    }
}
