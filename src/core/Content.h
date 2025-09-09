#pragma once
#include "url.h"
#include "textlist.h"
#include <wc.h>

extern char* index_file;
extern char* document_root;
extern char LocalhostOnly;
extern int retryAsHttp;

struct Content {
    struct Url pu;
    Str page;
    wc_ces charset;
    const char* real_type;
    TextList* document_header;
};

struct Form;
struct Content loadGeneralFile(const char* path, struct Url* current, struct Form* post,
    const char* referer);
