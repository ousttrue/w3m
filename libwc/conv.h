#pragma once
#include "Str.h"
#include "ces.h"

extern char* WcReplace;
extern char* WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

Str wc_Str_conv(Str is, enum wc_ces f_ces, enum wc_ces t_ces);
inline static Str wc_conv(const char* is, enum wc_ces f_ces, enum wc_ces t_ces)
{
    return wc_Str_conv(Strnew_charp(is), f_ces, t_ces);
}
inline static Str wc_conv_n(const char* is, size_t n, enum wc_ces f_ces, enum wc_ces t_ces)
{
    return wc_Str_conv(Strnew_charp_n(is, n), f_ces, t_ces);
}

Str wc_Str_conv_strict(Str is, enum wc_ces f_ces, enum wc_ces t_ces);
inline static Str wc_conv_strict(const char* is, enum wc_ces f_ces, enum wc_ces t_ces)
{
    return wc_Str_conv_strict(Strnew_charp(is), f_ces, t_ces);
}
inline static Str wc_conv_n_strict(const char* is, size_t n, enum wc_ces f_ces, enum wc_ces t_ces)
{
    return wc_Str_conv_strict(Strnew_charp_n(is, n), f_ces, t_ces);
}

struct Converted {
    Str os;
    enum wc_ces detected;
};
struct Converted wc_Str_conv_with_detect(Str is, enum wc_ces f_ces, enum wc_ces hint, enum wc_ces t_ces);
inline static Str wc_conv_with_detect(const char* is, enum wc_ces* f_ces, enum wc_ces hint, enum wc_ces t_ces)
{
    struct Converted converted = wc_Str_conv_with_detect(Strnew_charp(is),
        *f_ces, hint, t_ces);
    *f_ces = converted.detected;
    return converted.os;
}
inline static Str wc_conv_n_with_detect(const char* is, size_t n, enum wc_ces* f_ces, enum wc_ces hint, enum wc_ces t_ces)
{
    struct Converted converted = wc_Str_conv_with_detect(Strnew_charp_n(is, n),
        *f_ces, hint, t_ces);
    *f_ces = converted.detected;
    return converted.os;
}
