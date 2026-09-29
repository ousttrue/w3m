#pragma once
#include "Str.h"
#include "config.h"
#include "libwc/wc.h"
#include "charset.h"

#define FALSE 0
#define TRUE 1

extern char* getescapestr(char** s, int is_attr, int* pis_simple);
extern char* getescapecmd(char** s);

extern char* remove_space(char* str);

extern char* html_quote(char* str);
extern char* html_unquote(char* str);
extern char* html_unquote_attr(char* str);
extern char* file_quote(char* str);
extern char* file_unquote(char* str);
extern char* url_quote(const char* str);
static inline char* url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict(x, InnerCharset, c)->ptr);
}

extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), TRUE, FALSE)
extern const char* shell_quote(const char* str);

