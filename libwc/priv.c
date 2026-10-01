/* vi: set sw=4 ts=8 ai sm noet : */

#include "wc.h"
#include "wtf.h"

pStr
wc_conv_from_priv1(pStr is, wc_ces ces)
{
    pStr os;
    wc_uchar *sp = (wc_uchar *)is->ptr;
    wc_uchar *ep = sp + is->length;
    wc_uchar *p;
    wc_ccs ccs = WcCesInfo[WC_CCS_INDEX(ces)].gset[1].ccs;

    for (p = sp; p < ep && *p < 0x80; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->length);
    if (p > sp)
	Strcat_charp_n(os, is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	if (*p & 0x80)
	    wtf_push(os, ccs, (wc_uint32)*p);
	else
	    Strcat_char(os, (char)*p);
    }
    return os;
}

pStr
wc_char_conv_from_priv1(wc_uchar c, wc_status *st)
{
    pStr os = Strnew_size(1);

    if (c & 0x80)
	wtf_push(os, st->ces_info->gset[1].ccs, (wc_uint32)c);
    else
	Strcat_char(os, (char)c);
    return os;
}

pStr
wc_conv_from_ascii(pStr is, wc_ces ces)
{
    pStr os;
    wc_uchar *sp = (wc_uchar *)is->ptr;
    wc_uchar *ep = sp + is->length;
    wc_uchar *p;

    for (p = sp; p < ep && *p < 0x80; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->length);
    if (p > sp)
	Strcat_charp_n(os, is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	if (*p & 0x80)
	    wtf_push_unknown(os, p, 1);
	else
	    Strcat_char(os, (char)*p);
    }
    return os;
}

void
wc_push_to_raw(pStr os, wc_wchar_t cc, wc_status *st)
{

    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
    case WC_CCS_RAW:
	Strcat_char(os, (char)cc.code);
    }
    return;
}
