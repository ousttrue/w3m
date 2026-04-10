#include "wc_util.h"
#include "url.h"
#include "global.h"
#include <libwc/wtf.h>

// Don't change
wc_ces InnerCharset = (WC_CES_WTF);
#define DISPLAY_CHARSET WC_CES_UTF_8
wc_ces DisplayCharset = (DISPLAY_CHARSET);
#define DOCUMENT_CHARSET WC_CES_UTF_8
wc_ces DocumentCharset = (DOCUMENT_CHARSET);
#define SYSTEM_CHARSET WC_CES_UTF_8
wc_ces SystemCharset = (SYSTEM_CHARSET);
wc_ces BookmarkCharset = (SYSTEM_CHARSET);

struct wc_option WcOption = {
    .auto_detect = WC_OPT_DETECT_ON,
    .use_combining = WC_TRUE,
    .use_language_tag = WC_TRUE,
    .ucs_conv = WC_TRUE,
    .pre_conv = WC_FALSE,
    .fix_width_conv = WC_TRUE,
    .use_gb12345_map = WC_FALSE,
    .use_jisx0201 = WC_FALSE,
    .use_jisc6226 = WC_FALSE,
    .use_jisx0201k = WC_FALSE,
    .use_jisx0212 = WC_FALSE,
    .use_jisx0213 = WC_FALSE,
    .strict_iso2022 = WC_TRUE,
    .gb18030_as_ucs = WC_FALSE,
    .no_replace = WC_FALSE,
    .use_wide = WC_TRUE,
    .east_asian_width = WC_FALSE,
};

char* url_quote_conv(char* is, wc_ces c)
{
    return url_quote(wc_output__ptr(wc_conv_strict(WcOption, is, InnerCharset, c)));
}

Str Str_conv_from_system(const char* is, int len)
{
    return Strnew_wc_output(wc_Str_conv(WcOption, is, len, SystemCharset, InnerCharset));
}
Str Str_conv_to_system(const char* is, int len)
{
    return Strnew_wc_output(wc_Str_conv_strict(WcOption, is, len, InnerCharset, SystemCharset));
}
Str Str_conv_to_halfdump(struct wc_option opts, Str x)
{
    return ExtHalfdump
        ? Strnew_wc_output(wc_Str_conv(opts, (x)->ptr, (x)->length, InnerCharset, DisplayCharset))
        : x;
}
char* conv_from_system(const char* x)
{
    return Strnew_wc_output(wc_conv(WcOption, (x), SystemCharset, InnerCharset))->ptr;
}
char* conv_to_system(const char* x)
{
    return Strnew_wc_output(wc_conv_strict(WcOption, (x), InnerCharset, SystemCharset))->ptr;
}
int get_Str_strwidth(Str s)
{
    return wtf_strwidth(WcOption, (const wc_uchar*)(s->ptr));
}
int get_mcwidth(const char* c)
{
    return wtf_width(WcOption, *(const wc_uchar*)c);
}
