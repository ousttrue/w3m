#pragma once
#include "wc_types.h"
#include <string.h>

extern char* WcReplace;
extern char* WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

struct wc_output wc_Str_conv(struct wc_option opts, const char* is, int len, wc_ces f_ces, wc_ces t_ces);
struct wc_output wc_Str_conv_strict(struct wc_option opts, const char* is, int len, wc_ces f_ces, wc_ces t_ces);
struct wc_output wc_Str_conv_with_detect(struct wc_option opts, const uint8_t* is, int len, wc_ces* f_ces, wc_ces hint, wc_ces t_ces);

inline static struct wc_output wc_conv(struct wc_option opts, const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(opts, is, strlen(is), f_ces, t_ces);
}
inline static struct wc_output wc_conv_n(struct wc_option opts, const char* is, int n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv(opts, is, n, f_ces, t_ces);
}
inline static struct wc_output wc_conv_strict(struct wc_option opts, const char* is, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(opts, is, strlen(is), f_ces, t_ces);
}
inline static struct wc_output wc_conv_n_strict(struct wc_option opts, const char* is, int n, wc_ces f_ces, wc_ces t_ces)
{
    return wc_Str_conv_strict(opts, is, n, f_ces, t_ces);
}
inline static struct wc_output wc_conv_with_detect(struct wc_option opts, const uint8_t* is, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(opts, is, strlen((const char*)is), f_ces, hint, t_ces);
}
inline static struct wc_output wc_conv_n_with_detect(struct wc_option opts, const uint8_t* is, int n, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    return wc_Str_conv_with_detect(opts, is, n, f_ces, hint, t_ces);
}

void wc_char_conv_init(wc_ces f_ces, wc_ces t_ces);
struct wc_output wc_char_conv(struct wc_option opts, char c);
