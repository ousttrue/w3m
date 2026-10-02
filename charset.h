#pragma once
#include "config.h"
#include "str_gc.h"
#include "libwc/conv.h"
#include "libwc/wtf.h"

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
    return wc_Str_conv(WcOption, x, SystemCharset, InnerCharset);
}
static inline pStr Str_conv_to_system(struct wc_option* WcOption, pStr x)
{
    return wc_Str_conv_strict(WcOption, x, InnerCharset, SystemCharset);
}
static inline pStr Str_conv_to_halfdump(struct wc_option* WcOption, pStr x)
{
    return (ExtHalfdump ? wc_Str_conv(WcOption, x, InnerCharset, DisplayCharset) : x);
}
static inline char* conv_from_system(struct wc_option* WcOption, const char* x)
{
    return wc_conv(WcOption, (x), SystemCharset, InnerCharset)->ptr;
}
static inline char* conv_to_system(struct wc_option *WcOption, const char* x)
{
    return wc_conv_strict(WcOption, (x), InnerCharset, SystemCharset)->ptr;
}
