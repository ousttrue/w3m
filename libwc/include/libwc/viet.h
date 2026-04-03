#pragma once
#include "wc_types.h"

extern wc_uint8 wc_c0_tcvn57122_map[];
extern wc_uint8 wc_c0_viscii112_map[];
extern wc_uint8 wc_c0_vps2_map[];

struct wc_output wc_conv_from_viet(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_status;
void wc_push_to_viet(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
// void wc_push_to_cp1258(struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
struct wc_wchar wc_tcvn57123_to_tcvn5712(struct wc_wchar cc);
wc_uint32 wc_tcvn5712_precompose(wc_uchar c1, wc_uchar c2);
wc_uint32 wc_cp1258_precompose(wc_uchar c1, wc_uchar c2);
struct wc_output wc_char_conv_from_viet(struct wc_option opts, wc_uchar c, struct wc_status* st);
