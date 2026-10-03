#pragma once
#include "ces.h"

extern const uint8_t* WcReplace;
extern const uint8_t* WcReplaceW;
#define WC_REPLACE WcReplace
#define WC_REPLACE_W WcReplaceW

struct wc_option;
extern void wc_Str_conv(struct wc_option* WcOption, struct Writer *w, const uint8_t* sp, const uint8_t* ep, wc_ces f_ces, wc_ces t_ces);

extern void wc_Str_conv_strict(struct wc_option* WcOption, struct Writer *w, const uint8_t* sp, const uint8_t* ep, wc_ces f_ces, wc_ces t_ces);

extern void wc_Str_conv_with_detect(struct wc_option* WcOption, struct Writer *w, const uint8_t* sp, const uint8_t* ep, wc_ces* f_ces, wc_ces hint, wc_ces t_ces);

extern void wc_push_end(struct wc_option* WcOption, struct Writer *w, struct wc_status* st);
