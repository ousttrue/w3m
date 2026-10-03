#include "status.h"
#include "wtf.h"
#include "ccs.h"

void wc_conv_from_priv1(struct wc_option* WcOption, pStr os, const uint8_t *sp, const uint8_t *ep, wc_ces ces)
{
    wc_ccs ccs = WcCesInfo[WC_CCS_INDEX(ces)].gset[1].ccs;

    const uint8_t* p;
    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep){
        Strcopy_begin_end(os, sp, ep);
        return;
    }

    if (p > sp)
        Strcat_charp_n(os, (const char*)sp, (int)(p - sp));

    for (; p < ep; p++) {
        if (*p & 0x80)
            wtf_push(WcOption, os, ccs, (uint32_t)*p);
        else
            Strcat_char(os, (char)*p);
    }
}

void wc_char_conv_from_priv1(struct wc_option* WcOption, pStr os, uint8_t c, struct wc_status* st)
{
    if (c & 0x80)
        wtf_push(WcOption, os, st->ces_info->gset[1].ccs, (uint32_t)c);
    else
        Strcat_char(os, (char)c);
}

void wc_conv_from_ascii(struct wc_option* WcOption, pStr os, const uint8_t *sp, const uint8_t *ep, wc_ces ces)
{
    const uint8_t* p;
    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep) {
        Strcopy_begin_end(os, sp, ep);
        return;
    }

    if (p > sp)
        Strcat_charp_n(os, (const char*)sp, (int)(p - sp));

    for (; p < ep; p++) {
        if (*p & 0x80)
            wtf_push_unknown(WcOption, os, p, 1);
        else
            Strcat_char(os, (char)*p);
    }
}

void wc_push_to_raw(struct wc_option* WcOption, pStr os, wc_wchar_t cc, struct wc_status* st)
{

    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
    case WC_CCS_RAW:
        Strcat_char(os, (char)cc.code);
    }
    return;
}
