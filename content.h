#pragma once
#include "textlist.h"
#include "Str.h"
#include <stdbool.h>

struct Content {
    const char* filename;
    TextList* document_header;
};

bool matchattr(const char* p, const char* attr, int len, Str* value);
const char* checkHeader(struct Content content, const char* field);
const char* guess_filename(const char* file);
const char* guess_save_name(struct Content content, const char* file);
