#include "status.h"
#include "wtf.h"
#include "ccs.h"

pStr
wc_conv_from_priv1(pStr is, wc_ces ces)
{
    pStr os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->len;
    uint8_t *p;
    wc_ccs ccs = WcCesInfo[WC_CCS_INDEX(ces)].gset[1].ccs;

    for (p = sp; p < ep && *p < 0x80; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->len);
    if (p > sp)
	Strcat_charp_n(os, is->ptr, (int)(p - sp));

    for (; p < ep; p++) {
	if (*p & 0x80)
	    wtf_push(os, ccs, (uint32_t)*p);
	else
	    Strcat_char(os, (char)*p);
    }
    return os;
}

pStr
wc_char_conv_from_priv1(uint8_t c, struct wc_status *st)
{
    pStr os = Strnew_size(1);

    if (c & 0x80)
	wtf_push(os, st->ces_info->gset[1].ccs, (uint32_t)c);
    else
	Strcat_char(os, (char)c);
    return os;
}

pStr
wc_conv_from_ascii(pStr is, wc_ces ces)
{
    pStr os;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->len;
    uint8_t *p;

    for (p = sp; p < ep && *p < 0x80; p++)
	;
    if (p == ep)
	return is;
    os = Strnew_size(is->len);
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
wc_push_to_raw(pStr os, wc_wchar_t cc, struct wc_status *st)
{

    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
    case WC_CCS_RAW:
	Strcat_char(os, (char)cc.code);
    }
    return;
}
