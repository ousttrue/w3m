#pragma once

#include <gcstr/alloc.h>
#include <gcstr/Str.h>
#include <gcstr/myctype.h>
#include <gcstr/ctrlcode.h>
#include <gcstr/hash.h>
#include <gcstr/quote.h>
#include <wc/wc.h>
#include <stdarg.h>
#include <stdbool.h>

/// TODO: private internal
extern wc_ces InnerCharset; /* Don't change */

// TODO:
// const char*: immutable
// Str: mutable

int vscpf(const char* fmt, va_list ap);
Str base64_encode(const char* src, size_t len);
Str convert_size(long long size, bool usefloat);
Str convert_size2(long long size1, long long size2, bool usefloat);
bool matchattr(const char* p, const char* attr, int len, Str* value);
Str remove_space(const char* str);
Str mybasename(const char* s);
Str guess_filename(const char* file);
Str unescape_spaces(Str s);
Str lastFileName(const char* path);
Str url_quote(const char* str);
Str url_quote_conv(const char* x, wc_ces c);

char* html_quote(const char* str);
char* html_unquote(const char* str);
char* file_quote(char* str);
char* file_unquote(const char* str);
Str Str_url_unquote(Str x, int is_form, int safe);
Str Str_form_quote(Str x);
inline static Str Str_form_unquote(Str x) { return Str_url_unquote(x, true, false); }
char* shell_quote(char* str);
