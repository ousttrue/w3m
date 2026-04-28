#pragma once
#include "url.h"

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

struct HttpClient {
    struct Url message_sessions[FollowRedirection];
    int session_count;

    struct Url* current;
    const char* referer;
    enum UrlOptionFlags flag;
    struct Form* post;
};

void http_init(struct HttpClient* http);

enum HttpReidrectionStatus http_redirect(struct HttpClient* http, struct Url url);
