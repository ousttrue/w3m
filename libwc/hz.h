/* vi: set sw=4 ts=8 ai sm noet : */

#ifndef _WC_HZ_H
#define _WC_HZ_H

#define WC_C_HZ_TILDA	'~'
#define WC_C_HZ_SI	'{'
#define WC_C_HZ_SO	'}'

#define WC_HZ_NOSTATE	0
#define WC_HZ_TILDA	1
#define WC_HZ_TILDA_MB	2
#define WC_HZ_MBYTE	3
#define WC_HZ_MBYTE1	4
#define WC_HZ_MBYTE1_GR	5

extern pStr  wc_conv_from_hz(pStr is, wc_ces ces);
extern void wc_push_to_hz(pStr os, wc_wchar_t cc, wc_status *st);
extern void wc_push_to_hz_end(pStr os, wc_status *st);

#endif
