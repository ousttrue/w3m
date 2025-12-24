#pragma once
#include "Str.h"

Str unquote_mailcap(const char* qstr, const char* type, const char* name, const char* attr, int* mc_stat);

