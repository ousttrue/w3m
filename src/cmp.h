#pragma once
#include <string_view>

int strcasecmp(std::string_view l, std::string_view r);

#if defined(_MSC_VER)
int strncasecmp(const char *l, const char *r, size_t n);
int strcasecmp(const char *l, const char *r);
#else
#include <strings.h>
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
char *strcasestr(const char *str, const char *pattern);
#endif
