#pragma once
#include "url.h"
#include "http_request.h"
#include "UrlFile.h"

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

#define FollowRedirection 10

enum OpenStatus {
    HTST_UNKNOWN,
    HTST_NORMAL,
    HTST_CONNECT,
    HTST_MISSING,
};

/// HttpRequest and HttpResponse pair
struct HttpMessageSession {
    struct Url url;
    struct HttpRequest req;
    struct URLFile transport;
    enum OpenStatus transport_status;
};

struct HttpClient {
    struct HttpMessageSession message_sessions[FollowRedirection];
    int session_count;
    enum UrlOptionFlags flag;
};
void http_init(struct HttpClient* http, struct Url* current, enum UrlOptionFlags flag);
static inline struct Url* http_current(struct HttpClient *http)
{
    if(http->session_count==0){
        return NULL;
    }
    else{
        return &http->message_sessions[http->session_count-1].url;
    }
}
enum HttpReidrectionStatus http_redirect(struct HttpClient* http, const char* target,
    struct Form* post, const char* referer);
struct HttpMessageSession* http_open(struct HttpClient* http, struct CmdArgs* args, struct _textlist* extra_header);
