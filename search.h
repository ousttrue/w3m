#pragma once
#include <wc/wc.h>

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

char* conv_search_string(const char* str, wc_ces f_ces);
