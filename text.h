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
