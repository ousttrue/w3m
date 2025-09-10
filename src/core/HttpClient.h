#pragma once
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "UserInteraction.h"
#include <wc.h>

enum HttpConnectionStatus {
    HTST_UNKNOWN = 255,
    HTST_MISSING = 254,
    HTST_NORMAL = 0,
    HTST_CONNECT = 1,
};

#define MAX_FOLLOW_REDIRECTION 10

struct HttpExchange {
    enum HttpConnectionStatus status;
    union input_stream *stream;
    struct HttpRequest request;
    struct HttpResponse response;
};

struct HttpClient {
    struct UserInteraction ui;
    struct HttpExchange exchanges[MAX_FOLLOW_REDIRECTION];
    Str uname;
    Str pwd;
    Str realm;
    bool add_auth_cookie_flag;
    const char *ssl_certificate;
};

void httpInitClient(struct HttpClient* c, struct UserInteraction ui);
struct Form;
struct Content httpRequest(struct HttpClient* c,
    const char* path, struct Url* current, struct Form* post, const char* referer);
