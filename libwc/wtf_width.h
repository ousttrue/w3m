#pragma once

#include "wc_types.h"

void wtf_width_set(size_t i, wc_uint8 w);
wc_uint8 wtf_width(const wc_uchar* p);
inline static wc_uint8 get_mcwidth(const char* c)
{
    return wtf_width((const wc_uchar*)c);
}
