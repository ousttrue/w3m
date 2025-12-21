#pragma once
#include "Str.h"
#include "wc_types.h"

extern char* WcReplace;
extern char* WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

extern Str wc_Str_conv(Str is, wc_ces f_ces, wc_ces t_ces);
inline static Str wc_conv(const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(Strnew_charp(is), f_ces, t_ces);
}
inline static Str wc_conv_n(const char* is, size_t n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(Strnew_charp_n(is, n), f_ces, t_ces);
}

extern Str wc_Str_conv_strict(Str is, wc_ces f_ces, wc_ces t_ces);
inline static Str wc_conv_strict(const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(Strnew_charp(is), f_ces, t_ces);
}
inline static Str wc_conv_n_strict(const char* is, size_t n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(Strnew_charp_n(is, n), f_ces, t_ces);
}

extern Str wc_Str_conv_with_detect(Str is, wc_ces* f_ces, wc_ces hint, wc_ces t_ces);
inline static Str wc_conv_with_detect(const char* is, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(Strnew_charp(is), f_ces, hint, t_ces);
}
inline static Str wc_conv_n_with_detect(const char* is, size_t n, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(Strnew_charp_n(is, n), f_ces, hint, t_ces);
}
