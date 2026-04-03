#pragma once
#include "gbk.h"

#define WC_C_GB18030_UCS2 0x81308130U
#define WC_C_GB18030_UCS2_END 0x8431A439U
#define WC_C_GB18030_UCS4 0x90308130U
#define WC_C_GB18030_UCS4_END 0xE3329A35U

#define WC_GB18030_NOSTATE 0
#define WC_GB18030_MBYTE1 1 /* 0x81 - 0xA0, 0xA1 - 0xFE */
#define WC_GB18030_MBYTE2 2 /* 0x30 - 0x39 */
#define WC_GB18030_MBYTE3 3 /* 0x81 - 0xA0, 0xA1 - 0xFE */

#define WC_GB18030_MAP_C0 0x0
#define WC_GB18030_MAP_GL 0x1
#define WC_GB18030_MAP_C1 0x2
#define WC_GB18030_MAP_LB 0x4
#define WC_GB18030_MAP_UB (0x8 | WC_GB18030_MAP_LB)
#define WC_GB18030_MAP_L4 0x10

#define WC_GB18030_N(c)                     \
    (((((((c) >> 24) & 0xff) - 0x81) * 0x0A \
          + (((c) >> 16) & 0xff) - 0x30)    \
             * 0x7E                         \
         + (((c) >> 8) & 0xff) - 0x81)      \
            * 0x0A                          \
        + ((c) & 0xff) - 0x30)
#define WC_N_GB18030(c)                            \
    ((((c) / 0x0A / 0x7E / 0xA + 0x81) << 24)      \
        + (((c) / 0x0A / 0x7E % 0xA + 0x30) << 16) \
        + (((c) / 0x0A % 0x7E + 0x81) << 8)        \
        + (c) % 0xA + 0x30)

struct wc_wchar wc_gbk_ext_to_cs128w(struct wc_wchar cc);
struct wc_wchar wc_cs128w_to_gbk_ext(struct wc_wchar cc);
wc_uint32 wc_gb18030_to_ucs(struct wc_option opts, struct wc_wchar cc);
struct wc_wchar wc_ucs_to_gb18030(struct wc_option opts, wc_uint32 ucs);
struct wc_output wc_conv_from_gb18030(struct wc_option opts, const char* is, int len, wc_ces ces);
struct wc_status;
void wc_push_to_gb18030(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st);
struct wc_output wc_char_conv_from_gb18030(struct wc_option opts, wc_uchar c, struct wc_status* st);
