#pragma once
#include "wc_types.h"

#define WC_C_UTF7_PLUS '+'
#define WC_C_UTF7_MINUS '-'

#define WC_UTF7_MAP_SET_D 0x00
#define WC_UTF7_MAP_SET_O 0x01
#define WC_UTF7_MAP_SET_B 0x02
#define WC_UTF7_MAP_C0 0x04
#define WC_UTF7_MAP_C1 0x08
#define WC_UTF7_MAP_BASE64 0x10
#define WC_UTF7_MAP_PLUS 0x20
#define WC_UTF7_MAP_MINUS 0x40

#define WC_UTF7_NOSTATE 0
#define WC_UTF7_PLUS 1
#define WC_UTF7_BASE64 2

struct wc_output wc_conv_from_utf7(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_status;
void wc_push_to_utf7(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
void wc_push_to_utf7_end(struct wc_output* os, struct wc_status* st);
struct wc_output wc_char_conv_from_utf7(struct wc_option opts, wc_uchar c, struct wc_status* st);
