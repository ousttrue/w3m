#pragma once
#include "textlist.h"
#include "Str.h"
#include <stdbool.h>
#include <libwc/ces.h>

struct Content {
    const char* filename;
    int http_response_code;
    struct TextList* document_header;
    wc_ces content_charset;
    size_t current_content_length;
};

struct URLFile;
struct Url;
void getHttpResponseHeader(struct Content* content, struct URLFile* uf, struct Url* pu);

bool matchattr(const char* p, const char* attr, int len, Str* value);
const char* checkHeader(struct Content* content, const char* field);
const char* checkContentType(struct Content* content);
const char* guess_filename(const char* file);
const char* guess_save_name(struct Content* content, const char* file);
