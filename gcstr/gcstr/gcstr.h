#pragma once

#include "alloc.h"
#include "Str.h"
#include "myctype.h"
#include "ctrlcode.h"
#include "hash.h"
#include "quote.h"
#include <stdarg.h>
#include <stdbool.h>

int vscpf(const char* fmt, va_list ap);
Str base64_encode(const char* src, size_t len);
Str convert_size(long long size, bool usefloat);
Str convert_size2(long long size1, long long size2, bool usefloat);
bool matchattr(const char* p, const char* attr, int len, Str* value);
const char* remove_space(const char* str);
Str mybasename(const char* s);
Str guess_filename(const char* file);
Str unescape_spaces(Str s);
Str lastFileName(const char* path);
Str url_quote(const char* str);
