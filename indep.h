#pragma once
#include "Str.h"
#include "config.h"
#include "libwc/wc.h"
#include "charset.h"

#define FALSE 0
#define TRUE 1


extern Str remove_space(const char* str);

extern char* html_quote(const char* str);

extern Str Str_form_quote(Str x);
extern const char* shell_quote(const char* str);

