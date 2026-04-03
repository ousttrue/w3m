#pragma once
#include "wc_types.h"

#define WC_C_HZ_TILDA '~'
#define WC_C_HZ_SI '{'
#define WC_C_HZ_SO '}'

#define WC_HZ_NOSTATE 0
#define WC_HZ_TILDA 1
#define WC_HZ_TILDA_MB 2
#define WC_HZ_MBYTE 3
#define WC_HZ_MBYTE1 4
#define WC_HZ_MBYTE1_GR 5

struct wc_output wc_conv_from_hz(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_status;
void wc_push_to_hz(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
void wc_push_to_hz_end(struct wc_output* os, struct wc_status* st);
