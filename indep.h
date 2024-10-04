#pragma once
#include "Str.h"
#include <stdint.h>

extern int strCmp(const void *s1, const void *s2);
extern char *currentdir(void);


enum CLEANUP_LINE_MODE {
  RAW_MODE = 0,
  PAGER_MODE = 1,
  HTML_MODE = 2,
  HEADER_MODE = 3,
};
extern void cleanup_line(Str s, enum CLEANUP_LINE_MODE mode);
extern Str convertLine(Str line, enum CLEANUP_LINE_MODE mode);
// #define convertLine(uf, line, mode, charset, dcharset)                         \
//   convertLine0(uf, line, mode)

extern const char *html_quote(const char *str);
extern const char *file_quote(const char *str);
extern const char *file_unquote(const char *str);

extern Str Str_url_unquote(Str x, bool is_form, bool safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
extern const char *shell_quote(const char *str);


