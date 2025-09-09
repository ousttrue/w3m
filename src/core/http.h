#pragma once
#include "http_request_method.h"
#include "textlist.h"
#include <Str.h>
#include <wc.h>

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
extern int accept_cookie;
extern int show_cookie;
enum AcceptBadCookieMode {
    ACCEPT_BAD_COOKIE_DISCARD = 0,
    ACCEPT_BAD_COOKIE_ACCEPT = 1,
    ACCEPT_BAD_COOKIE_ASK = 2,
};
extern enum AcceptBadCookieMode accept_bad_cookie;

#define NO_REFERER ((const char*)-1)

enum HttpRequestFlag {
    HR_FLAG_LOCAL = 1,
    HR_FLAG_PROXY = 2,
};

struct Form;

struct HttpRequest {
    enum HttpMethod method;
    enum HttpRequestFlag flag;
    const char* referer;
    struct Form* request;
};
struct Url;

Str getHttpRequestURIStr(struct Url* pu, struct HttpRequest* hr);
Str getHttpRequestStr(struct Url* pu, struct Url* current, struct HttpRequest* hr, TextList* extra);

bool matchattr(const char* p, const char* attr, int len, Str* value);
const char* getHttpHeaderValue(TextList* document_header, const char* field);

struct ContentTypeCharset {
    const char* content_type;
    wc_ces charset;
};
struct ContentTypeCharset getContentType(TextList* document_header);
const char* guessFileName(const char* file);
const char* mybasename(const char* s);
const char* guessSaveName(TextList* document_header, const char* file);

bool is_text_type(const char* type);
bool is_plain_text_type(const char* type);
bool is_html_type(const char* type);
