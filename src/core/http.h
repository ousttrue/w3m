#pragma once
#include "url.h"
#include "textlist.h"
#include <Str.h>
#include <wc.h>

extern int accept_cookie;
extern int show_cookie;
enum AcceptBadCookieMode {
    ACCEPT_BAD_COOKIE_DISCARD = 0,
    ACCEPT_BAD_COOKIE_ACCEPT = 1,
    ACCEPT_BAD_COOKIE_ASK = 2,
};
extern enum AcceptBadCookieMode accept_bad_cookie;

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
