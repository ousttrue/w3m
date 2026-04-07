#pragma once
#include "wc_types.h"

#define WTF_TYPE_ASCII 0x0
#define WTF_TYPE_CTRL 0x1
#define WTF_TYPE_WCHAR1 0x2
#define WTF_TYPE_WCHAR2 0x4
#define WTF_TYPE_WIDE 0x8
#define WTF_TYPE_UNKNOWN 0x10
#define WTF_TYPE_UNDEF 0x20
#define WTF_TYPE_WCHAR1W (WTF_TYPE_WCHAR1 | WTF_TYPE_WIDE)
#define WTF_TYPE_WCHAR2W (WTF_TYPE_WCHAR2 | WTF_TYPE_WIDE)

void wtf_init(wc_ces ces1, wc_ces ces2);
int wtf_width(struct wc_option opts, wc_uchar p);
int wtf_strwidth(struct wc_option opts, const wc_uchar* p);
size_t wtf_len1(wc_uchar* p);
size_t wtf_len(const wc_uchar* p);
int wtf_type(wc_uchar* p);
// #define wtf_type(p) WTF_TYPE_MAP[(wc_uchar) * (p)]

void wtf_push(struct wc_option opts, struct wc_output* os, wc_ccs ccs, wc_uint32 code);
void wtf_push_unknown(struct wc_option opts, struct wc_output* os, wc_uchar* p, size_t len);
struct wc_wchar wtf_parse(struct wc_option opts, wc_uchar** p);
struct wc_wchar wtf_parse1(wc_uchar** p);
wc_ccs wtf_get_ccs(wc_uchar* p);
wc_uint32 wtf_get_code(wc_uchar* p);
wc_bool wtf_is_hangul(wc_uchar* p);
char* wtf_conv_fit(struct wc_option opts, char* s, wc_ces ces);

#define get_mctype(c) ((Lineprop)wtf_type((wc_uchar*)(c)) << 8)
#define get_mclen(c) wtf_len1((wc_uchar*)(c))
// #define get_mcwidth(c) wtf_width((wc_uchar*)(c))
static inline int get_strwidth(struct wc_option opts, const char* c) { return wtf_strwidth(opts, (const wc_uchar*)c); }
// #define get_Str_strwidth(c) wtf_strwidth((wc_uchar*)((c)->ptr))
