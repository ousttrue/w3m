#pragma once
#include <gcstr.h>
#include <stdint.h>

extern char UseAltEntity;

const char* conv_entity(uint32_t ch);

/// return decoded char. -1 is error
int getescapechar(const char** s);

/// escape a character(some bytes).
Str getescapecmd(const char** s);

