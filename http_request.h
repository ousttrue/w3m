#pragma once
#include "Str.h"
#include "textlist.h"

enum HttpRequestMethod {
    HR_COMMAND_GET = 0,
    HR_COMMAND_POST = 1,
    HR_COMMAND_CONNECT = 2,
    HR_COMMAND_HEAD = 3,
};

enum HttpRequetFlags {
    HR_FLAG_LOCAL = 1,
    HR_FLAG_PROXY = 2,
};

struct HttpRequest {
    enum HttpRequestMethod command;
    enum HttpRequetFlags flag;
    char* referer;
    struct FormList* request;
};

Str HTTPrequestMethod(struct HttpRequest* hr);
struct Url;
Str HTTPrequestURI(struct Url* pu, struct HttpRequest* hr);
Str HTTPrequest(struct Url* pu, struct Url* current, struct HttpRequest* hr, struct TextList* extra);
