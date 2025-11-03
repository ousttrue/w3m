#pragma once
#include "libwc/wc.h"

extern wc_ces InnerCharset; /* Don't change */
extern wc_ces DisplayCharset;
extern wc_ces DocumentCharset;
extern wc_ces SystemCharset;
extern wc_ces BookmarkCharset;

#define Str_conv_from_system(x) wc_Str_conv((x), SystemCharset, InnerCharset)
#define Str_conv_to_system(x) wc_Str_conv_strict((x), InnerCharset, SystemCharset)
#define Str_conv_to_halfdump(x) (ExtHalfdump ? wc_Str_conv((x), InnerCharset, DisplayCharset) : (x))
#define conv_from_system(x) wc_conv((x), SystemCharset, InnerCharset)->ptr
#define conv_to_system(x) wc_conv_strict((x), InnerCharset, SystemCharset)->ptr
#define url_quote_conv(x, c) url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr)
