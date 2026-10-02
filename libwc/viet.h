#pragma once
#include "ces.h"

extern uint8_t wc_c0_tcvn57122_map[];
extern uint8_t wc_c0_viscii112_map[];
extern uint8_t wc_c0_vps2_map[];

extern pStr wc_conv_from_viet(struct wc_option *WcOption, pStr is, wc_ces ces);
struct wc_option;
extern void wc_push_to_viet(struct wc_option* WcOption, pStr os, wc_wchar_t cc, struct wc_status* st);
extern void wc_push_to_cp1258(pStr os, wc_wchar_t cc, struct wc_status* st);
extern wc_wchar_t wc_tcvn57123_to_tcvn5712(wc_wchar_t cc);
extern uint32_t wc_tcvn5712_precompose(uint8_t c1, uint8_t c2);
extern uint32_t wc_cp1258_precompose(uint8_t c1, uint8_t c2);
extern pStr wc_char_conv_from_viet(struct wc_option *WcOption, uint8_t c, struct wc_status* st);
