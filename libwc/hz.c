#include "conv.h"
#include "status.h"
#include "iso2022.h"
#include "hz.h"
#include "wtf.h"
#include "ucs.h"

void wc_conv_from_hz(struct wc_option *WcOption, struct Writer *w, const uint8_t *sp, const uint8_t *ep, wc_ces ces)
{
    int state = WC_HZ_NOSTATE;

    const uint8_t* p;
    for (p = sp; p < ep && *p < 0x80 && *p != WC_C_HZ_TILDA; p++)
        ;
    if (p == ep){
        CALL2(w, setBeginEnd, sp, ep);
        return;
    }

    if (p > sp)
        CALL2(w, pushStrLen, sp, (int)(p - sp));

    for (; p < ep; p++) {
        switch (state) {
        case WC_HZ_NOSTATE:
            if (*p == WC_C_HZ_TILDA)
                state = WC_HZ_TILDA;
            else if (WC_ISO_MAP[*p] == WC_ISO_MAP_GR)
                state = WC_HZ_MBYTE1_GR; /* GB 2312 ? */
            else if (*p & 0x80)
                wtf_push_unknown(WcOption, w, p, 1);
            else
                WRITER_PUSH_CH(w,*p);
            break;
        case WC_HZ_TILDA:
            if (*p == WC_C_HZ_SI) {
                state = WC_HZ_MBYTE;
                break;
            } else if (*p == WC_C_HZ_TILDA)
                WRITER_PUSH_CH(w,*p);
            else if (*p == '\n')
                break;
            else
                wtf_push_unknown(WcOption, w, p - 1, 2);
            state = WC_HZ_NOSTATE;
            break;
        case WC_HZ_TILDA_MB:
            if (*p == WC_C_HZ_SO || *p == '\n') {
                state = WC_HZ_NOSTATE;
                break;
            } else if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
                wtf_push(WcOption, w, WC_CCS_GB_2312, ((uint32_t)*(p - 1) << 8) | *p);
            else
                wtf_push_unknown(WcOption, w, p - 1, 2);
            state = WC_HZ_MBYTE;
            break;
        case WC_HZ_MBYTE:
            if (*p == WC_C_HZ_TILDA)
                state = WC_HZ_TILDA_MB;
            else if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
                state = WC_HZ_MBYTE1;
            else
                wtf_push_unknown(WcOption, w, p, 1);
            break;
        case WC_HZ_MBYTE1:
            if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
                wtf_push(WcOption, w, WC_CCS_GB_2312, ((uint32_t)*(p - 1) << 8) | *p);
            else
                wtf_push_unknown(WcOption, w, p - 1, 2);
            state = WC_HZ_MBYTE;
            break;
        case WC_HZ_MBYTE1_GR:
            if (WC_ISO_MAP[*p] == WC_ISO_MAP_GR)
                wtf_push(WcOption, w, WC_CCS_GB_2312, ((uint32_t)*(p - 1) << 8) | *p);
            else
                wtf_push_unknown(WcOption, w, p - 1, 2);
            state = WC_HZ_NOSTATE;
            break;
        }
    }
    switch (state) {
    case WC_HZ_TILDA:
    case WC_HZ_TILDA_MB:
    case WC_HZ_MBYTE1:
    case WC_HZ_MBYTE1_GR:
        wtf_push_unknown(WcOption, w, p - 1, 1);
        break;
    }
}

void wc_push_to_hz(struct wc_option *WcOption, struct Writer *w, wc_wchar_t cc, struct wc_status* st)
{
    while (1) {
        switch (cc.ccs) {
        case WC_CCS_US_ASCII:
            if (st->gl) {
                WRITER_PUSH_CH(w,WC_C_HZ_TILDA);
                WRITER_PUSH_CH(w,WC_C_HZ_SO);
                st->gl = 0;
            }
            if ((char)cc.code == WC_C_HZ_TILDA)
                WRITER_PUSH_CH(w,WC_C_HZ_TILDA);
            WRITER_PUSH_CH(w,cc.code);
            return;
        case WC_CCS_GB_2312:
            if (!st->gl) {
                WRITER_PUSH_CH(w,WC_C_HZ_TILDA);
                WRITER_PUSH_CH(w,WC_C_HZ_SI);
                st->gl = 1;
            }
            WRITER_PUSH_CH(w,((cc.code >> 8) & 0x7f));
            WRITER_PUSH_CH(w,(cc.code & 0x7f));
            return;
        case WC_CCS_UNKNOWN_W:
            if (WcOption->no_replace)
                return;
            if (st->gl) {
                WRITER_PUSH_CH(w,WC_C_HZ_TILDA);
                WRITER_PUSH_CH(w,WC_C_HZ_SO);
                st->gl = 0;
            }
            CALL1(w, pushStr, WC_REPLACE_W);
            return;
        case WC_CCS_UNKNOWN:
            if (WcOption->no_replace)
                return;
            if (st->gl) {
                WRITER_PUSH_CH(w,WC_C_HZ_TILDA);
                WRITER_PUSH_CH(w,WC_C_HZ_SO);
                st->gl = 0;
            }
            CALL1(w, pushStr, WC_REPLACE);
            return;
        default:
            if (WcOption->ucs_conv)
                cc = wc_any_to_any_ces(WcOption, cc, st);
            else
                cc.ccs = WC_CCS_IS_WIDE(cc.ccs) ? WC_CCS_UNKNOWN_W : WC_CCS_UNKNOWN;
            continue;
        }
    }
}

void wc_push_to_hz_end(struct wc_option* WcOption, struct Writer *w, struct wc_status* st)
{
    if (st->gl) {
        WRITER_PUSH_CH(w,WC_C_HZ_TILDA);
        WRITER_PUSH_CH(w,WC_C_HZ_SO);
        st->gl = 0;
    }
}
