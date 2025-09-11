#pragma once
#include <stddef.h>
#include <stdbool.h>

struct CharSlice {
    const char* p;
    size_t len;
};

struct CharSlice makeSlice(const char* p);
bool startswith(const char* p, struct CharSlice slice);
