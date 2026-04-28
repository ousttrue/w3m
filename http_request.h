#pragma once
#include "Str.h"

enum HttpMethod {
    HR_COMMAND_GET = 0,
    HR_COMMAND_POST = 1,
    HR_COMMAND_CONNECT = 2,
    HR_COMMAND_HEAD = 3,
};

enum HttpRequestFlags {
    HR_FLAG_LOCAL = 1,
    HR_FLAG_PROXY = 2,
};

struct HttpRequest {
    enum HttpMethod http_method;
    enum HttpRequestFlags flag;
    const char* referer;
    struct Form* request;
};

static inline const char* HTTPrequestMethod(struct HttpRequest* hr)
{
    switch (hr->http_method) {
    case HR_COMMAND_CONNECT:
        return "CONNECT";
    case HR_COMMAND_POST:
        return "POST";
    case HR_COMMAND_HEAD:
        return "HEAD";
    case HR_COMMAND_GET:
        return "GET";
    }
    return NULL;
}

struct Url;
Str HTTPrequestURI(struct Url* pu, struct HttpRequest* hr);
struct _textlist;
Str HTTPrequest(struct Url* pu, struct Url* current, struct HttpRequest* hr, struct _textlist* extra);


