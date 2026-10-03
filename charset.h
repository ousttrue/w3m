#pragma once
#include "str_gc.h"
#include "libwc/conv.h"

extern wc_ces DisplayCharset;
extern wc_ces DocumentCharset;
extern wc_ces BookmarkCharset;
extern char ExtHalfdump;
extern char FollowLocale;
extern char UseContentCharset;
extern char SearchConv;
extern char SimplePreserveSpace;

static inline pStr Str_conv_from_system(struct wc_option* WcOption, pStr x)
{
    pStr os = Strnew_size(x->len);
    wc_Str_conv(WcOption, os,
        (const uint8_t*)x->ptr, (const uint8_t*)x->ptr + x->len, SystemCharset, InnerCharset);
    return os;
}
static inline pStr Str_conv_to_system(struct wc_option* WcOption, pStr x)
{
    pStr os = Strnew_size(x->len);
    wc_Str_conv_strict(WcOption, os,
        (const uint8_t*)x->ptr, (const uint8_t*)x->ptr + x->len, InnerCharset, SystemCharset);
    return os;
}
static inline pStr Str_conv_to_halfdump(struct wc_option* WcOption, pStr x)
{
    pStr os = Strnew_size(x->len);
    if (ExtHalfdump)
        wc_Str_conv(WcOption, os,
            (const uint8_t*)x->ptr, (const uint8_t*)x->ptr + x->len, InnerCharset, DisplayCharset);
    else
        Strcopy(os, x);
    return os;
}
static inline void conv_from_system(struct wc_option* WcOption, pStr os, const char* is)
{
    wc_Str_conv(WcOption, os, (const uint8_t*)is, (const uint8_t*)is + strlen(is), SystemCharset, InnerCharset);
}
static inline void conv_to_system(struct wc_option* WcOption, pStr os, const char* is)
{
    wc_Str_conv_strict(WcOption, os,
        (const uint8_t*)is,
        (const uint8_t*)is + strlen(is), InnerCharset, SystemCharset);
}
