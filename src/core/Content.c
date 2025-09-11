#include "Content.h"
#include "http_message.h"
#include "mailcap.h"
#include "runtime.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "istream.h"
#include "keymap.h"
#include "cookie.h"
#include "network.h"
#include "time_util.h"
#include "alloc.h"
#include "ContentType.h"
#include "html_form.h"
#include "HttpClient.h"
#include "ssl_util.h"
#include "local_cgi.h"
#include "myctype.h"
#include "proxy.h"
#include "auth.h"
#include "quote.h"
#include "URLFile.h"

#include <openssl/ssl.h>
#include <unistd.h>
#include <zlib.h>
#include <assert.h>

char* index_file = 0;
char LocalhostOnly = false;
int retryAsHttp = true;

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
            // pu->real_file = q;
            return;
        }
    }
}

struct Content openLocal(const char* u, struct Url* current, struct Form* post, const char* referer)
{
    // u = file_to_url(u);
    struct Url pu;
    pu = parseUrl(u, current);

    if (pu.label != NULL) {
        // #hogege is not a label but a filename
        Str tmp2 = Strnew_charp("#");
        Strcat_charp(tmp2, pu.label);
        pu.file = tmp2->ptr;
        // pu.real_file = cleanupName(file_unquote(pu.file));
        pu.label = NULL;
    }

    struct URLFile f;
    init_stream(&f, SCM_MISSING, NULL);
    if (post && post->body) {
        // local CGI: POST
        f.stream = newFileStream(localcgi_post(pu.file, pu.query, post, referer), &fclose);
    } else {
        // lodal CGI: GET
        f.stream = newFileStream(localcgi_get(pu.file, pu.query, referer), &fclose);
    }

    if (f.stream) {
        f.is_cgi = true;
        f.scheme = pu.scheme = SCM_LOCAL_CGI;
    } else {
        examineFile(&f, pu.file);
        if (f.stream == NULL) {
            if (dir_exist(pu.file)) {
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
                    // pu.real_file = q;
                    add_index_file(&pu, &f);
                    // if (f.stream == NULL) {
                    //     // return;
                    // }
                } else {
                    examineFile(&f, q);
                    if (f.stream) {
                        pu.file = p;
                        // pu.real_file = q;
                    }
                }
            }
        }
    }
    if (!f.stream) {
        return (struct Content) {
            .url = pu,
            .page = NULL,
        };
    }
    Str page = readAll(f.stream);

    enum ContentType content_type = guessContentType(pu.file);
    if (content_type == NULL) {
        content_type = CONTENTTYPE_TEXT_PLAIN;
    }
    // if (f.guess_type) {
    //     content_type = f.guess_type;
    // }

    // term_raw();
    return (struct Content) {
        .url = pu,
        .page = page,
        .cc = {
            .charset = WC_CES_UTF_8,
            .content_type = content_type,
        },
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

struct Content
loadGeneralFile(const char* path, struct Url* current, struct Form* post, const char* referer,
    struct UserInteraction ui)
{
    //         openURL(&c, &pu, current, post, referer, no_cache, extra_header, &hr);
    // void openURL(struct HttpClient* c, struct Url* pu, struct Url* current,
    //     struct Form* post, const char* referer, bool no_cache, TextList* extra_header,
    //     struct HttpRequest* hr)

    struct Url pu;
    pu = parseUrl(path, current);

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
            httpInitClient(&c, ui);
            return httpRequest(&c, Strnew_m_charp("http://", path, NULL)->ptr, current, post, referer);
            //     //     }
        }
    }

    case SCM_HTTP:
    case SCM_HTTPS: {
        struct HttpClient c;
        httpInitClient(&c, ui);
        return httpRequest(&c, path, current, post, referer);
    }

    default:
        return (struct Content) {};
    }
}
