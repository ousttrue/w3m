#pragma once
#include "ces.h"

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

extern uint8_t WC_UTF7_MAP[];
struct wc_option;
extern void wc_conv_from_utf7(struct wc_option* WcOption, pStr os, pStr is, wc_ces ces);
extern void wc_push_to_utf7(struct wc_option* WcOption, pStr os, wc_wchar_t cc, struct wc_status* st);
extern void wc_push_to_utf7_end(struct wc_option* WcOption, pStr os, struct wc_status* st);
extern void wc_char_conv_from_utf7(struct wc_option* WcOption, pStr os, uint8_t c, struct wc_status* st);
