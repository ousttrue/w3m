#pragma once
#include "Str.h"
#include <stdint.h>

bool is_boundary(const unsigned char *ch1, const unsigned char *ch2);

const char *convert_size(int64_t size, int usefloat);
const char *convert_size2(int64_t size1, int64_t size2, int usefloat);

const char *remove_space(const char *str);
Str unescape_spaces(Str s);

const char *cleanupName(const char *name);

const char *getWord(const char **str);

