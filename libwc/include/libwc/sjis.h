#pragma once
#include "wc_types.h"

#define WC_C_SJIS_ERROR 0xFFFFFFFFU

#define WC_SJIS_NOSTATE 0
#define WC_SJIS_SHIFT_L 1 /* 0xA1 - 0xBF */
#define WC_SJIS_SHIFT_H 2 /* 0xE0 - 0xEF */
#define WC_SJIS_SHIFT_X 3 /* 0xF0 - 0xFC (JIS X 0213-2) */

#define WC_SJIS_MAP_C0 0x0
#define WC_SJIS_MAP_GL 0x1
#define WC_SJIS_MAP_LB 0x10
#define WC_SJIS_MAP_UB 0x20
#define WC_SJIS_MAP_80 (0x2 | WC_SJIS_MAP_LB)
#define WC_SJIS_MAP_SK (0x3 | WC_SJIS_MAP_LB)
#define WC_SJIS_MAP_SL (0x4 | WC_SJIS_MAP_UB | WC_SJIS_MAP_LB)
#define WC_SJIS_MAP_SH (0x5 | WC_SJIS_MAP_UB | WC_SJIS_MAP_LB)
#define WC_SJIS_MAP_SX (0x6 | WC_SJIS_MAP_UB | WC_SJIS_MAP_LB)
#define WC_SJIS_MAP_A0 (0x7 | WC_SJIS_MAP_LB)
#define WC_SJIS_MAP_C1 0x40

extern wc_uint8 WC_SJIS_MAP[];

struct wc_wchar wc_sjis_to_jis(struct wc_wchar cc);
struct wc_wchar wc_jis_to_sjis(struct wc_wchar cc);
struct wc_wchar wc_sjis_ext_to_cs94w(struct wc_wchar cc);
struct wc_wchar wc_cs94w_to_sjis_ext(struct wc_wchar cc);
wc_uint32 wc_sjis_ext1_to_N(wc_uint32 cc);
wc_uint32 wc_sjis_ext2_to_N(wc_uint32 cc);
struct wc_output wc_conv_from_sjis(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_output wc_conv_from_sjisx0213(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_status;
void wc_push_to_sjis(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
void wc_push_to_sjisx0213(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
struct wc_output wc_char_conv_from_sjis(struct wc_option opts, wc_uchar c, struct wc_status* st);
struct wc_output wc_char_conv_from_sjisx0213(struct wc_option opts, wc_uchar c, struct wc_status* st);
