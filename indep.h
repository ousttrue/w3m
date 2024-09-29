#pragma once
#include "Str.h"
#include <stdint.h>

extern int64_t strtoclen(const char *s);
extern int strCmp(const void *s1, const void *s2);
extern char *currentdir(void);
extern char *cleanupName(char *name);
extern char *expandPath(char *name);
// #ifndef HAVE_STRCHR
// extern char *strchr(const char *s, int c);
// #endif /* not HAVE_STRCHR */

extern char *remove_space(char *str);
extern int non_null(char *s);

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

extern char *html_quote(char *str);
extern char *file_quote(char *str);
extern char *file_unquote(char *str);
extern char *url_quote(char *str);
#define url_quote_conv(x, c) url_quote(x)
#define url_encode(url, base, cs) url_quote(url)

extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
extern const char *shell_quote(const char *str);

extern char *w3m_auxbin_dir();
extern char *w3m_lib_dir();
extern char *w3m_etc_dir();
extern char *w3m_conf_dir();
extern char *w3m_help_dir();
