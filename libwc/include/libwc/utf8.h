#pragma once
#include "wc_types.h"

#define WC_C_UTF8_L2 0x80
#define WC_C_UTF8_L3 0x800
#define WC_C_UTF8_L4 0x10000
#define WC_C_UTF8_L5 0x200000
#define WC_C_UTF8_L6 0x4000000

#define WC_UTF8_NOSTATE 0
#define WC_UTF8_NEXT 1

extern wc_uint8 WC_UTF8_MAP[];

size_t wc_ucs_to_utf8(wc_uint32 ucs, wc_uchar* utf8);
wc_uint32 wc_utf8_to_ucs(wc_uchar* utf8);
struct wc_output wc_conv_from_utf8(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_status;
void wc_push_to_utf8(struct wc_option opts, struct wc_output *os, struct wc_wchar cc, struct wc_status* st);
void wc_push_to_utf8_end(struct wc_output *os, struct wc_status* st);
struct wc_output wc_char_conv_from_utf8(struct wc_option opts, wc_uchar c, struct wc_status* st);
