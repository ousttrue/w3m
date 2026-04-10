#pragma once
#include <string.h>

static inline int strCmp(const void* s1, const void* s2)
{
    return strcmp(*(const char**)s1, *(const char**)s2);
}
