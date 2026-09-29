/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_INDEP_H
#define W3M_INDEP_H

#include "Str.h"
#include "config.h"
#include "libwc/wc.h"
#include "charset.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) > (b) ? (b) : (a))

#define FALSE 0
#define TRUE 1

extern size_t strtoclen(const char* s);
extern char* conv_entity(unsigned int ch);
extern char* getescapestr(char** s, int is_attr, int* pis_simple);
extern char* getescapecmd(char** s);
extern char* allocStr(const char* s, int len);
extern int strCmp(const void* s1, const void* s2);
extern char* currentdir(void);
#ifndef HAVE_STRCASESTR
extern char* strcasestr(const char* s1, const char* s2);
#endif
int strmatchlen(const char* s1, const char* s2, int maxlen);
extern char* remove_space(char* str);
extern int non_null(const char* s);
extern char* html_quote(char* str);
extern char* html_unquote(char* str);
extern char* html_unquote_attr(char* str);
extern char* file_quote(char* str);
extern char* file_unquote(char* str);
extern char* url_quote(char* str);
static inline char* url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict(x, InnerCharset, c)->ptr);
}

extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), TRUE, FALSE)
extern const char* shell_quote(const char* str);

extern char* CurrentDir;
extern int CurrentPid;

#endif
