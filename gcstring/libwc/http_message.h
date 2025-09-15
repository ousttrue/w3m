#pragma once
#include "textlist.h"
#include "CharSlice.h"
#include <wc.h>

const char* getHttpHeaderValue(TextList* document_header, const char* field);

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
