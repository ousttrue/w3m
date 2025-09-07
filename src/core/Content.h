#pragma once
#include "url.h"
#include "istream.h"

struct Content {
    struct Url pu;
    Str page;
    wc_ces charset;
    const char* real_type;
    TextList* document_header;
};

struct form_list;
struct Content loadGeneralFile(const char* path, struct Url* current, struct form_list* post,
    const char* referer, bool no_cache);
