#pragma once
#include "ces.h"

#define WC_C_HZ_TILDA '~'
#define WC_C_HZ_SI '{'
#define WC_C_HZ_SO '}'

#define WC_HZ_NOSTATE 0
#define WC_HZ_TILDA 1
#define WC_HZ_TILDA_MB 2
#define WC_HZ_MBYTE 3
#define WC_HZ_MBYTE1 4
#define WC_HZ_MBYTE1_GR 5

extern void wc_conv_from_hz(struct wc_option *WcOption, struct Writer *w, const uint8_t *sp, const uint8_t *ep, wc_ces ces);
struct wc_option;
extern void wc_push_to_hz(struct wc_option* WcOption, struct Writer *w, wc_wchar_t cc, struct wc_status* st);
extern void wc_push_to_hz_end(struct wc_option* WcOption, struct Writer *w, struct wc_status* st);
