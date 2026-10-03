#pragma once
#include "ces.h"

extern char* WcReplace;
extern char* WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

struct wc_option;
extern void wc_Str_conv(struct wc_option* WcOption, pStr os, const uint8_t* sp, const uint8_t* ep, wc_ces f_ces, wc_ces t_ces);
static inline pStr wc_conv(struct wc_option* WcOption, const char* sp, wc_ces f_ces, wc_ces t_ces)
{
    int len = strlen(sp);
    const char* ep = sp + len;
    pStr os = Strnew_size(len);
    wc_Str_conv(WcOption, os, (const uint8_t*)sp, (const uint8_t*)ep, f_ces, t_ces);
    return os;
}
static inline pStr wc_conv_n(struct wc_option* WcOption, const char* sp, int len, wc_ces f_ces, wc_ces t_ces)
{
    const char* ep = sp + len;
    pStr os = Strnew_size(len);
    wc_Str_conv(WcOption, os, (const uint8_t*)sp, (const uint8_t*)ep, f_ces, t_ces);
    return os;
}

extern pStr wc_Str_conv_strict(struct wc_option* WcOption, const uint8_t* sp, const uint8_t* ep, wc_ces f_ces, wc_ces t_ces);
static inline pStr wc_conv_strict(struct wc_option* WcOption, const char* sp, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(WcOption, (const uint8_t*)sp, (const uint8_t*)sp + strlen(sp), f_ces, t_ces);
}
static inline pStr wc_conv_n_strict(struct wc_option* WcOption, const char* sp, int len, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(WcOption, (const uint8_t*)sp, (const uint8_t*)sp + len, f_ces, t_ces);
}

extern pStr wc_Str_conv_with_detect(struct wc_option* WcOption, const uint8_t* sp, const uint8_t* ep, wc_ces* f_ces, wc_ces hint, wc_ces t_ces);
static inline pStr wc_conv_with_detect(struct wc_option* WcOption, const char* sp, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(WcOption, (const uint8_t*)sp, (const uint8_t*)sp + strlen(sp), f_ces, hint, t_ces);
}
static inline pStr wc_conv_n_with_detect(struct wc_option* WcOption, const char* sp, int len, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(WcOption, (const uint8_t*)sp, (const uint8_t*)sp + len, f_ces, hint, t_ces);
}

extern void wc_push_end(struct wc_option* WcOption, pStr os, struct wc_status* st);
