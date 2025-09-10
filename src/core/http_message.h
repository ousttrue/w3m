#pragma once
#include <Str.h>
#include <stdbool.h>

/// extract attr=value from p;
bool matchattr(const char* p, const char* attr, int len, Str* value);
