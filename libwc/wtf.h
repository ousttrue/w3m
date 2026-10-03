#pragma once
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

#define WTF_TYPE_ASCII 0x0
#define WTF_TYPE_CTRL 0x1
#define WTF_TYPE_WCHAR1 0x2
#define WTF_TYPE_WCHAR2 0x4
#define WTF_TYPE_WIDE 0x8
#define WTF_TYPE_UNKNOWN 0x10
#define WTF_TYPE_UNDEF 0x20
#define WTF_TYPE_WCHAR1W (WTF_TYPE_WCHAR1 | WTF_TYPE_WIDE)
#define WTF_TYPE_WCHAR2W (WTF_TYPE_WCHAR2 | WTF_TYPE_WIDE)

extern uint8_t WTF_WIDTH_MAP[];
extern uint8_t WTF_LEN_MAP[];
extern uint8_t WTF_TYPE_MAP[];
extern wc_ccs wtf_gr_ccs;

extern void wtf_init(wc_ces ces1, wc_ces ces2);

extern int wtf_width(struct wc_option *WcOption, const uint8_t* p);
extern int wtf_strwidth(struct wc_option *WcOption, const uint8_t* p);
extern size_t wtf_len1(const uint8_t* p);
extern size_t wtf_len(const uint8_t* p);
/* extern int     wtf_type(uint8_t *p); */
#define wtf_type(p) WTF_TYPE_MAP[(uint8_t)*(p)]

extern void wtf_push(struct wc_option *WcOption, struct Writer *w, wc_ccs ccs, uint32_t code);
extern void wtf_push_unknown(struct wc_option *WcOption, struct Writer *w, const uint8_t* p, size_t len);
extern wc_wchar_t wtf_parse(struct wc_option *WcOption, const uint8_t** p);
extern wc_wchar_t wtf_parse1(const uint8_t** p);

extern wc_ccs wtf_get_ccs(const uint8_t* p);
extern uint32_t wtf_get_code(const uint8_t* p);

extern bool wtf_is_hangul(const uint8_t* p);

extern void wtf_conv_fit(struct wc_option *WcOption, struct Writer *w, const uint8_t* s, wc_ces ces);
