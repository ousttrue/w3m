#pragma once
#include "url.h"
#include "HttpRequestMethod.h"
#include "textlist.h"

// extern bool override_user_agent;
extern const char* UserAgent;
extern char* AcceptMedia;
extern char* AcceptEncoding;
extern char* AcceptLang;
extern char NoCache;
extern int NoSendReferer;
extern int CrossOriginReferer;
// extern int override_content_type;

#define NO_REFERER ((const char*)-1)

enum HttpRequestFlag {
    HR_FLAG_LOCAL = 1,
    HR_FLAG_PROXY = 2,
};

struct Form;

struct HttpRequest {
    struct Url url;
    enum HttpMethod method;
    enum HttpRequestFlag flag;
    const char* referer;
    struct Form* post;
};

Str getHttpRequestURIStr(struct HttpRequest* hr);
Str getHttpRequestStr(struct HttpRequest* hr, struct Url* current, TextList* extra);
