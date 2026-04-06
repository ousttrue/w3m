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
    struct form_list* request;
};

Str HTTPrequestMethod(struct HttpRequest* hr);
struct Url;
Str HTTPrequestURI(struct Url* pu, struct HttpRequest* hr);
struct _textlist;
Str HTTPrequest(struct Url* pu, struct Url* current, struct HttpRequest* hr, struct _textlist* extra);

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1
