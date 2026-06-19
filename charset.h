#ifndef W3M_CHARSET_H
#define W3M_CHARSET_H

#include "config.h"
#ifdef USE_M17N
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

#define Str_conv_from_system(x) wc_Str_conv((x), SystemCharset, InnerCharset)
#define Str_conv_to_system(x) wc_Str_conv_strict((x), InnerCharset, SystemCharset)
#define Str_conv_to_halfdump(x) (ExtHalfdump ? wc_Str_conv((x), InnerCharset, DisplayCharset) : (x))
#define conv_from_system(x) wc_conv((x), SystemCharset, InnerCharset)->ptr
#define conv_to_system(x) wc_conv_strict((x), InnerCharset, SystemCharset)->ptr
#define url_quote_conv(x,c) url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr)
#else
#define Str_conv_from_system(x) (x)
#define Str_conv_to_system(x) (x)
#define Str_conv_to_halfdump(x) (x)
#define conv_from_system(x) (x)
#define conv_to_system(x) (x)
#define url_quote_conv(x,c) url_quote(x)
#define wc_Str_conv(x,charset0,charset1) (x)
#define wc_Str_conv_strict(x,charset0,charset1) (x)
#endif
#endif
