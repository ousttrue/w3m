#pragma once
#include "Str.h"
#include <stdint.h>

extern int64_t strtoclen(const char *s);
extern char *conv_entity(unsigned int ch);
extern int getescapechar(char **s);
extern char *getescapecmd(char **s);
extern int strCmp(const void *s1, const void *s2);
extern char *currentdir(void);
extern char *cleanupName(char *name);
extern char *expandPath(char *name);
// #ifndef HAVE_STRCHR
// extern char *strchr(const char *s, int c);
// #endif /* not HAVE_STRCHR */

#ifdef _WIN32
extern char *strcasestr(const char *s1, const char *s2);
#else
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

extern int strcasemstr(char *str, char *srch[], char **ret_ptr);
int strmatchlen(const char *s1, const char *s2, int maxlen);
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
extern char *html_unquote(char *str);
extern char *file_quote(char *str);
extern char *file_unquote(char *str);
extern char *url_quote(char *str);
extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
extern char *shell_quote(char *str);

extern char *w3m_auxbin_dir();
extern char *w3m_lib_dir();
extern char *w3m_etc_dir();
extern char *w3m_conf_dir();
extern char *w3m_help_dir();
