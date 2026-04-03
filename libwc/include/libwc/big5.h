#pragma once
#include "wc_types.h"

#define WC_C_BIG5_2_BASE (0x28 * 0x9D)

#define WC_BIG5_NOSTATE 0
#define WC_BIG5_MBYTE1 1 /* 0xA1 - 0xFE */

#define WC_BIG5_MAP_C0 0x0
#define WC_BIG5_MAP_GL 0x1
#define WC_BIG5_MAP_C1 0x2
#define WC_BIG5_MAP_LB 0x4
#define WC_BIG5_MAP_UB (0x3 | WC_BIG5_MAP_LB)

#define WC_BIG5UL_N(U, L) (((U) - 0xA1) * 0x9D \
    + (L) - (((L) < 0xA1) ? 0x40 : 0x62))
#define WC_BIG5_N(c) WC_BIG5UL_N(((c) >> 8) & 0xFF, (c) & 0xFF)
#define WC_N_BIG5U(c) ((c) / 0x9D + 0xA1)
#define WC_N_BIG5L(c) ((c) % 0x9D + (((c) % 0x9D < 0x3F) ? 0x40 : 0x62))
#define WC_N_BIG5(c) ((WC_N_BIG5U(c) << 8) + WC_N_BIG5L(c))

extern wc_uchar WC_BIG5_MAP[];

struct wc_wchar wc_big5_to_cs94w(struct wc_wchar cc);
struct wc_wchar wc_cs94w_to_big5(struct wc_wchar cc);
struct wc_output wc_conv_from_big5(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_status;
void wc_push_to_big5(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
struct wc_output wc_char_conv_from_big5(struct wc_option opts, wc_uchar c, struct wc_status* st);
