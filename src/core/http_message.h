#pragma once
#include "textlist.h"
#include <stdbool.h>
#include <stddef.h>
#include <wc.h>

struct CharSlice {
    const char* p;
    size_t len;
};

struct CharSlice makeSlice(const char* p);
bool startswith(const char* p, struct CharSlice slice);
const char* getHttpHeaderValue(TextList* document_header, const char* field);

struct ContentTypeCharset {
    const char* content_type;
    wc_ces charset;
};
struct ContentTypeCharset getContentType(TextList* document_header);
const char* guessFileName(const char* file);
const char* guessSaveName(TextList* document_header, const char* file);

/// extract attr=value from p;
struct CharSlice extractSemiColon(const char* p, struct CharSlice attr);
inline static bool matchattr(const char* p, struct CharSlice attr, struct CharSlice* value)
{
    struct CharSlice slice = extractSemiColon(p, attr);
    if (slice.p) {
        if (value) {
            *value = (struct CharSlice) { slice.p };
        }
        return true;
    } else {
        return false;
    }
}
