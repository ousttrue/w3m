#pragma once
#include "ces.h"

extern char* WcReplace;
extern char* WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

struct wc_option;
extern pStr wc_Str_conv(struct wc_option* WcOption, pStr is, wc_ces f_ces, wc_ces t_ces);
static inline pStr wc_conv(struct wc_option* WcOption, const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(WcOption, Strnew_charp(is), f_ces, t_ces);
}
static inline pStr wc_conv_n(struct wc_option* WcOption, const char* is, int n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(WcOption, Strnew_charp_n(is, n), f_ces, t_ces);
}

extern pStr wc_Str_conv_strict(struct wc_option *WcOption, pStr is, wc_ces f_ces, wc_ces t_ces);
static inline pStr wc_conv_strict(struct wc_option *WcOption, const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(WcOption, Strnew_charp(is), f_ces, t_ces);
}
static inline pStr wc_conv_n_strict(struct wc_option *WcOption, const char* is, int n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(WcOption, Strnew_charp_n(is, n), f_ces, t_ces);
}

extern pStr wc_Str_conv_with_detect(struct wc_option* WcOption, pStr is, wc_ces* f_ces, wc_ces hint, wc_ces t_ces);
static inline pStr wc_conv_with_detect(struct wc_option* WcOption, const char* is, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(WcOption, Strnew_charp(is), f_ces, hint, t_ces);
}
static inline pStr wc_conv_n_with_detect(struct wc_option* WcOption, const char* is, int n, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(WcOption, Strnew_charp_n(is, n), f_ces, hint, t_ces);
}

extern void wc_push_end(struct wc_option* WcOption, pStr os, struct wc_status* st);
