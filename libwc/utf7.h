#pragma once
#include "ces.h"
#include "Str.h"

#define WC_C_UTF7_PLUS	'+'
#define WC_C_UTF7_MINUS	'-'

#define WC_UTF7_MAP_SET_D	0x00
#define WC_UTF7_MAP_SET_O	0x01
#define WC_UTF7_MAP_SET_B	0x02
#define WC_UTF7_MAP_C0		0x04
#define WC_UTF7_MAP_C1		0x08
#define WC_UTF7_MAP_BASE64	0x10
#define WC_UTF7_MAP_PLUS	0x20
#define WC_UTF7_MAP_MINUS	0x40

#define WC_UTF7_NOSTATE		0
#define WC_UTF7_PLUS		1
#define WC_UTF7_BASE64		2

extern wc_uint8 WC_UTF7_MAP[];

extern Str       wc_conv_from_utf7(Str is, enum wc_ces ces);
struct wc_status;
extern void      wc_push_to_utf7(Str os, wc_wchar_t cc, struct wc_status *st);
extern void      wc_push_to_utf7_end(Str os, struct wc_status *st);
extern Str       wc_char_conv_from_utf7(wc_uchar c, struct wc_status *st);

