#pragma once
#include <stdbool.h>
#include <stddef.h>

struct CharSlice {
    const char* p;
    size_t len;
};

struct CharSlice makeSlice(const char* p);

bool startswith(const char* p, struct CharSlice slice);

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
