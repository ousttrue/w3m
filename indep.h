#pragma once
#include "Str.h"
#include "config.h"
#include "libwc/wc.h"
#include "charset.h"

#define FALSE 0
#define TRUE 1

extern char* getescapestr(char** s, int is_attr, int* pis_simple);
extern char* getescapecmd(char** s);

extern Str remove_space(const char* str);

extern char* html_quote(const char* str);
extern char* html_unquote(const char* str);
extern char* html_unquote_attr(const char* str);

extern Str Str_form_quote(Str x);
extern const char* shell_quote(const char* str);

