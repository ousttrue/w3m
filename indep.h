#pragma once
#include "Str.h"
#include <stdint.h>


extern Str Str_url_unquote(Str x, bool is_form, bool safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
extern const char *shell_quote(const char *str);
