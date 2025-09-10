#pragma once
#include "url.h"
#include "textlist.h"
#include "UserInteraction.h"
#include <wc.h>

extern char* index_file;
extern char LocalhostOnly;
extern int retryAsHttp;

struct Content {
    struct Url url;
    Str page;
    const char* content_type;
    wc_ces charset;
};

struct Form;
struct Content loadGeneralFile(const char* path, struct Url* current, struct Form* post,
    const char* referer, struct UserInteraction ui);

bool is_text_type(const char* type);
bool is_plain_text_type(const char* type);
bool is_html_type(const char* type);
