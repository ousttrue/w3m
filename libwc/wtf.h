#pragma once
#include "wc_types.h"
#include "ces.h"

#define WTF_C_CS94 0x80
#define WTF_C_CS94W 0x81
#define WTF_C_CS96 0x82
#define WTF_C_CS96W 0x83 /* reserved */
#define WTF_C_CS942 0x84
#define WTF_C_UNKNOWN 0x85
#define WTF_C_PCS 0x86
#define WTF_C_PCSW 0x87
#define WTF_C_WCS16 0x88
#define WTF_C_WCS16W 0x89
#define WTF_C_WCS32 0x8A
#define WTF_C_WCS32W 0x8B

#define WTF_C_COMB 0x10
#define WTF_C_CS94_C (WTF_C_CS94 | WTF_C_COMB) /* reserved */
#define WTF_C_CS94W_C (WTF_C_CS94W | WTF_C_COMB) /* reserved */
#define WTF_C_CS96_C (WTF_C_CS96 | WTF_C_COMB) /* reserved */
#define WTF_C_CS96W_C (WTF_C_CS96W | WTF_C_COMB) /* reserved */
#define WTF_C_CS942_C (WTF_C_CS942 | WTF_C_COMB) /* reserved */
#define WTF_C_PCS_C (WTF_C_PCS | WTF_C_COMB)
#define WTF_C_PCSW_C (WTF_C_PCSW | WTF_C_COMB) /* reserved */
#define WTF_C_WCS16_C (WTF_C_WCS16 | WTF_C_COMB)
#define WTF_C_WCS16W_C (WTF_C_WCS16W | WTF_C_COMB) /* reserved */
#define WTF_C_WCS32_C (WTF_C_WCS32 | WTF_C_COMB) /* reserved */
#define WTF_C_WCS32W_C (WTF_C_WCS32W | WTF_C_COMB) /* reserved */

#define WTF_C_UNDEF0 0x8C
#define WTF_C_UNDEF1 0x8D
#define WTF_C_UNDEF2 0x8E
#define WTF_C_UNDEF3 0x8F
#define WTF_C_UNDEF4 0x9C
#define WTF_C_UNDEF5 0x9D
#define WTF_C_UNDEF6 0x9E
#define WTF_C_UNDEF7 0x9F
#define WTF_C_NBSP 0xA0

void wtf_init(enum wc_ces ces1, enum wc_ces ces2);

void wtf_push(Str os, wc_ccs ccs, wc_uint32 code);
void wtf_push_unknown(Str os, wc_uchar* p, size_t len);
wc_wchar_t wtf_parse(wc_uchar** p);
wc_wchar_t wtf_parse1(wc_uchar** p);

wc_ccs wtf_get_ccs(wc_uchar* p);
wc_uint32 wtf_get_code(wc_uchar* p);

wc_bool wtf_is_hangul(wc_uchar* p);

char* wtf_conv_fit(const char* s, enum wc_ces ces);
