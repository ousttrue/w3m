#pragma once
#include "config.h"

#include "libwc/wc.h"
#include "libwc/wc_types.h"
#include "libwc/wtf.h"

extern const wc_ces InnerCharset;
extern wc_ces DisplayCharset;
extern wc_ces DocumentCharset;
extern wc_ces SystemCharset;
extern wc_ces BookmarkCharset;
extern char ExtHalfdump;
extern char FollowLocale;
extern char UseContentCharset;
extern char SearchConv;
extern char SimplePreserveSpace;

static inline Str Str_conv_from_system(Str x)
{
    return wc_Str_conv(x, SystemCharset, InnerCharset);
}
static inline Str Str_conv_to_system(Str x)
{
    return wc_Str_conv_strict(x, InnerCharset, SystemCharset);
}
static inline Str Str_conv_to_halfdump(Str x)
{
    return (ExtHalfdump ? wc_Str_conv(x, InnerCharset, DisplayCharset) : x);
}
static inline char* conv_from_system(const char* x)
{
    return wc_conv((x), SystemCharset, InnerCharset)->ptr;
}
static inline char* conv_to_system(const char* x)
{
    return wc_conv_strict((x), InnerCharset, SystemCharset)->ptr;
}
