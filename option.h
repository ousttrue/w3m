#pragma once
#include "Str.h"
#include <stdbool.h>

bool opt_set_param(const char* name, const char* value);
bool opt_set_param_option(const char* option);
char* opt_get_param_option(const char* name);
Str opt_load_panel(void);
