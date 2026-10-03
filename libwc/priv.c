#include "status.h"
#include "wtf.h"
#include "ccs.h"

void wc_conv_from_priv1(struct wc_option* WcOption, struct Writer *w, const uint8_t *sp, const uint8_t *ep, wc_ces ces)
{
    wc_ccs ccs = WcCesInfo[WC_CCS_INDEX(ces)].gset[1].ccs;

    const uint8_t* p;
    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep){
        CALL2(w, setBeginEnd, sp, ep);
        return;
    }

    if (p > sp)
        CALL2(w, pushStrLen, sp, (int)(p - sp));

    for (; p < ep; p++) {
        if (*p & 0x80)
            wtf_push(WcOption, w, ccs, (uint32_t)*p);
        else
            WRITER_PUSH_CH(w,*p);
    }
}

void wc_char_conv_from_priv1(struct wc_option* WcOption, struct Writer *w, uint8_t c, struct wc_status* st)
{
    if (c & 0x80)
        wtf_push(WcOption, w, st->ces_info->gset[1].ccs, (uint32_t)c);
    else
        WRITER_PUSH_CH(w,c);
}

void wc_conv_from_ascii(struct wc_option* WcOption, struct Writer *w, const uint8_t *sp, const uint8_t *ep, wc_ces ces)
{
    const uint8_t* p;
    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep) {
        CALL2(w, setBeginEnd, sp, ep);
        return;
    }

    if (p > sp)
        CALL2(w, pushStrLen, sp, (int)(p - sp));

    for (; p < ep; p++) {
        if (*p & 0x80)
            wtf_push_unknown(WcOption, w, p, 1);
        else
            WRITER_PUSH_CH(w,*p);
    }
}

void wc_push_to_raw(struct wc_option* WcOption, struct Writer *w, wc_wchar_t cc, struct wc_status* st)
{

    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
    case WC_CCS_RAW:
        WRITER_PUSH_CH(w,cc.code);
    }
    return;
}
