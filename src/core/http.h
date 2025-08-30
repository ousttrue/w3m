#pragma once
#include <Str.h>
#include "textlist.h"

extern int override_user_agent;
extern char* UserAgent;
extern char* AcceptMedia;
extern char* AcceptEncoding;
extern char* AcceptLang;
extern char NoCache;
extern int NoSendReferer;
extern int CrossOriginReferer;
extern int use_cookie;
extern int override_content_type;
extern Str header_string;

#define NO_REFERER ((const char*)-1)

enum HttpMethod {
    HR_COMMAND_GET = 0,
    HR_COMMAND_POST = 1,
    HR_COMMAND_CONNECT = 2,
    HR_COMMAND_HEAD = 3,
};

enum HttpRequestFlag {
    HR_FLAG_LOCAL = 1,
    HR_FLAG_PROXY = 2,
};

struct form_list;
struct HttpRequest {
    enum HttpMethod command;
    enum HttpRequestFlag flag;
    const char* referer;
    struct form_list* request;
};
struct _ParsedURL;

Str HTTPrequestMethod(struct HttpRequest* hr);
Str HTTPrequestURI(struct _ParsedURL* pu, struct HttpRequest* hr);
Str HTTPrequest(struct _ParsedURL* pu, struct _ParsedURL* current, struct HttpRequest* hr, TextList* extra);
