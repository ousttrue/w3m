#pragma once
#include "Str.h"
#include <stdint.h>

int64_t strtoclen(const char *s);
bool non_null(const char *s);
bool is_boundary(const unsigned char *ch1, const unsigned char *ch2);

const char *convert_size(int64_t size, int usefloat);
const char *convert_size2(int64_t size1, int64_t size2, int usefloat);

const char *remove_space(const char *str);
Str unescape_spaces(Str s);

const char *getWord(const char **str);
const char *getQWord(const char **str);

enum CLEANUP_LINE_MODE {
  RAW_MODE = 0,
  PAGER_MODE = 1,
  HTML_MODE = 2,
  HEADER_MODE = 3,
};
void cleanup_line(Str s, enum CLEANUP_LINE_MODE mode);
Str convertLine(Str line, enum CLEANUP_LINE_MODE mode);
const char *cleanupName(const char *name);

#define HAVE_STRCASECMP 1
#define HAVE_STRCASESTR 1
#define HAVE_STRCHR 1
#define HAVE_STRERROR 1

#ifdef _WIN32
extern char *strcasestr(const char *s1, const char *s2);
#else
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
extern int strcasemstr(char *str, char *srch[], char **ret_ptr);
