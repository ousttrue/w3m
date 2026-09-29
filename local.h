#pragma once
#include "Str.h"

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)
#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

Str loadLocalDir(const char* dirname);
struct form_list;
FILE* localcgi_post(const char* uri,
    const char* query, struct form_list* post, const char* referer);
static inline FILE* localcgi_get(const char* uri, const char* query, const char* referer)
{
    return localcgi_post(uri, query, 0, referer);
}
