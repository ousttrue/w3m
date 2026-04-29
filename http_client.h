#pragma once
#include "url.h"
#include "http_request.h"
#include "UrlFile.h"
#include "http_response.h"
#include <libwc/wc_types.h>

enum UrlOptionFlags {
    RG_NOCACHE = 1,
    RG_FRAME = 2,
    RG_FRAME_SRC = 4,
};

enum HttpReidrectionStatus {
    HTTP_REDIRECTION_OK,
    HTTP_REDIRECTION_EXCEEDED,
    HTTP_REDIRECTION_LOOP_DETECTED,
};

enum OpenStatus {
    HTST_UNKNOWN,
    HTST_NORMAL,
    HTST_CONNECT,
    HTST_MISSING,
};

/// HttpRequest and HttpResponse pair
struct HttpMessageSession {
    // request
    struct Url url;
    struct HttpRequest req;
    // connect
    struct URLFile transport;
    enum OpenStatus transport_status;
    // response
    int64_t current_content_length;
    wc_ces content_charset;
    struct HttpResponse res;
    const char* t; // = "text/plain";
    const char* real_type; // = NULL;
    Str page; // = NULL;
    wc_ces charset; // = WC_CES_US_ASCII;
    const char* header_source;
    // auth
    struct _textlist* extra_header; // = newTextList();
    Str uname; // = NULL;
    Str pwd; // = NULL;
    Str realm; // = NULL;
    bool add_auth_cookie_flag; // = false;
    struct Url* auth_pu; //= NULL;
};

#define FollowRedirection 10

struct HttpClient {
    /// message_sessions(message stack)
    ///
    /// current  [2] => Url: HttpRequest => HttpResponse (redirection)
    /// base     [1] => Url: HttpRequest => HttpResponse (first get)
    /// base_url [0] => Url
    ///
    struct HttpMessageSession message_sessions[FollowRedirection];
    int session_count;

    enum UrlOptionFlags flag;
    bool searchHeader; //= SearchHeader;
    bool searchHeader_through; //= true;
    bool has_base_url;
};
void http_init(struct HttpClient* http, struct Url* base_url, enum UrlOptionFlags flag);
/// session_count-1
static inline struct HttpMessageSession* http_session_current(struct HttpClient* http)
{
    if (http->session_count < 1) {
        return NULL;
    }
    return &http->message_sessions[http->session_count - 1];
}
/// session_count-2 or NULL
static inline struct HttpMessageSession* http_session_base(struct HttpClient* http)
{
    if (http->session_count < 2) {
        return NULL;
    }
    return &http->message_sessions[http->session_count - 2];
}
struct HttpMessageSession* http_redirect(struct HttpClient* http, const char* target,
    struct Form* post, const char* referer);
void http_open(struct HttpClient* http, struct CmdArgs* args);

struct HttpClient http_get(struct CmdArgs* args, const char* path, struct Url* base_url, struct Form* post,
    const char* referer,
    enum UrlOptionFlags flag);
