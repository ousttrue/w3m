#pragma once
#include "ces.h"

extern char* WcReplace;
extern char* WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

extern pStr wc_Str_conv(pStr is, wc_ces f_ces, wc_ces t_ces);
static inline pStr wc_conv(const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(Strnew_charp(is), f_ces, t_ces);
}
static inline pStr wc_conv_n(const char* is, int n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(Strnew_charp_n(is, n), f_ces, t_ces);
}

extern pStr wc_Str_conv_strict(pStr is, wc_ces f_ces, wc_ces t_ces);
static inline pStr wc_conv_strict(const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(Strnew_charp(is), f_ces, t_ces);
}
static inline pStr wc_conv_n_strict(const char* is, int n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(Strnew_charp_n(is, n), f_ces, t_ces);
}

extern pStr wc_Str_conv_with_detect(pStr is, wc_ces* f_ces, wc_ces hint, wc_ces t_ces);
static inline pStr wc_conv_with_detect(const char* is, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(Strnew_charp(is), f_ces, hint, t_ces);
}
static inline pStr wc_conv_n_with_detect(const char* is, int n, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(Strnew_charp_n(is, n), f_ces, hint, t_ces);
}

extern void wc_char_conv_init(wc_ces f_ces, wc_ces t_ces);
extern pStr wc_char_conv(char c);
extern void wc_push_end(pStr os, wc_status* st);
