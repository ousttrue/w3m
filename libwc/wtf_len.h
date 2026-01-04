#pragma once
#include "wc_types.h"

void wtf_len_set(size_t i, wc_uint8 l);
size_t wtf_len1(wc_uchar* p);
inline static size_t get_mclen(const char* c)
{
    return wtf_len1((wc_uchar*)(c));
}
size_t wtf_len(const wc_uchar* p);
int wtf_strwidth(wc_uchar* p);
inline static int get_strwidth(const char* c) { return wtf_strwidth((wc_uchar*)(c)); }
// #define get_Str_strwidth(c) wtf_strwidth((wc_uchar*)((c)->ptr))
