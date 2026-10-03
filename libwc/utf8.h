#pragma once
#include "ces.h"

#define WC_C_UTF8_L2 0x80
#define WC_C_UTF8_L3 0x800
#define WC_C_UTF8_L4 0x10000
#define WC_C_UTF8_L5 0x200000
#define WC_C_UTF8_L6 0x4000000

#define WC_UTF8_NOSTATE 0
#define WC_UTF8_NEXT 1

extern uint8_t WC_UTF8_MAP[];

struct wc_option;
extern size_t wc_ucs_to_utf8(uint32_t ucs, uint8_t* utf8);
extern uint32_t wc_utf8_to_ucs(const uint8_t* utf8);
extern void wc_conv_from_utf8(struct wc_option* WcOption, struct Writer *w, const uint8_t *sp, const uint8_t *ep, wc_ces ces);
extern void wc_push_to_utf8(struct wc_option* WcOption, struct Writer *w, wc_wchar_t cc, struct wc_status* st);
extern void wc_push_to_utf8_end(struct wc_option* WcOption, struct Writer *w, struct wc_status* st);
extern void wc_char_conv_from_utf8(struct wc_option* WcOption, struct Writer *w, uint8_t c, struct wc_status* st);
