#include "hz.h"
#include "conv.h"
#include "status.h"
#include "ccs.h"
#include "iso2022.h"
#include "wtf.h"
#include "ucs.h"

struct wc_output wc_conv_from_hz(struct wc_option opts, const char* is, int len, wc_ces ces)
{
    wc_uchar* sp = (wc_uchar*)is;
    wc_uchar* ep = sp + len;
    wc_uchar* p;
    int state = WC_HZ_NOSTATE;

    for (p = sp; p < ep && *p < 0x80 && *p != WC_C_HZ_TILDA; p++)
        ;
    if (p == ep)
        return wc_output_from_charp(is);
    struct wc_output os = wc_output_from_size(len);
    if (p > sp)
        wc_output__cat_charp_n(&os, is, (int)(p - sp));

    for (; p < ep; p++) {
        switch (state) {
        case WC_HZ_NOSTATE:
            if (*p == WC_C_HZ_TILDA)
                state = WC_HZ_TILDA;
            else if (WC_ISO_MAP[*p] == WC_ISO_MAP_GR)
                state = WC_HZ_MBYTE1_GR; /* GB 2312 ? */
            else if (*p & 0x80)
                wtf_push_unknown(opts, &os, p, 1);
            else
                wc_output__cat_char(&os, (char)*p);
            break;
        case WC_HZ_TILDA:
            if (*p == WC_C_HZ_SI) {
                state = WC_HZ_MBYTE;
                break;
            } else if (*p == WC_C_HZ_TILDA)
                wc_output__cat_char(&os, (char)*p);
            else if (*p == '\n')
                break;
            else
                wtf_push_unknown(opts, &os, p - 1, 2);
            state = WC_HZ_NOSTATE;
            break;
        case WC_HZ_TILDA_MB:
            if (*p == WC_C_HZ_SO || *p == '\n') {
                state = WC_HZ_NOSTATE;
                break;
            } else if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
                wtf_push(opts, &os, WC_CCS_GB_2312, ((wc_uint32) * (p - 1) << 8) | *p);
            else
                wtf_push_unknown(opts, &os, p - 1, 2);
            state = WC_HZ_MBYTE;
            break;
        case WC_HZ_MBYTE:
            if (*p == WC_C_HZ_TILDA)
                state = WC_HZ_TILDA_MB;
            else if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
                state = WC_HZ_MBYTE1;
            else
                wtf_push_unknown(opts, &os, p, 1);
            break;
        case WC_HZ_MBYTE1:
            if (WC_ISO_MAP[*p & 0x7f] == WC_ISO_MAP_GL)
                wtf_push(opts, &os, WC_CCS_GB_2312, ((wc_uint32) * (p - 1) << 8) | *p);
            else
                wtf_push_unknown(opts, &os, p - 1, 2);
            state = WC_HZ_MBYTE;
            break;
        case WC_HZ_MBYTE1_GR:
            if (WC_ISO_MAP[*p] == WC_ISO_MAP_GR)
                wtf_push(opts, &os, WC_CCS_GB_2312, ((wc_uint32) * (p - 1) << 8) | *p);
            else
                wtf_push_unknown(opts, &os, p - 1, 2);
            state = WC_HZ_NOSTATE;
            break;
        }
    }
    switch (state) {
    case WC_HZ_TILDA:
    case WC_HZ_TILDA_MB:
    case WC_HZ_MBYTE1:
    case WC_HZ_MBYTE1_GR:
        wtf_push_unknown(opts, &os, p - 1, 1);
        break;
    }
    return os;
}

void wc_push_to_hz(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st)
{
    while (1) {
        switch (cc.ccs) {
        case WC_CCS_US_ASCII:
            if (st->gl) {
                wc_output__cat_char(os, WC_C_HZ_TILDA);
                wc_output__cat_char(os, WC_C_HZ_SO);
                st->gl = 0;
            }
            if ((char)cc.code == WC_C_HZ_TILDA)
                wc_output__cat_char(os, WC_C_HZ_TILDA);
            wc_output__cat_char(os, (char)cc.code);
            return;
        case WC_CCS_GB_2312:
            if (!st->gl) {
                wc_output__cat_char(os, WC_C_HZ_TILDA);
                wc_output__cat_char(os, WC_C_HZ_SI);
                st->gl = 1;
            }
            wc_output__cat_char(os, (char)((cc.code >> 8) & 0x7f));
            wc_output__cat_char(os, (char)(cc.code & 0x7f));
            return;
        case WC_CCS_UNKNOWN_W:
            if (opts.no_replace)
                return;
            if (st->gl) {
                wc_output__cat_char(os, WC_C_HZ_TILDA);
                wc_output__cat_char(os, WC_C_HZ_SO);
                st->gl = 0;
            }
            wc_output__cat_charp(os, WC_REPLACE_W);
            return;
        case WC_CCS_UNKNOWN:
            if (opts.no_replace)
                return;
            if (st->gl) {
                wc_output__cat_char(os, WC_C_HZ_TILDA);
                wc_output__cat_char(os, WC_C_HZ_SO);
                st->gl = 0;
            }
            wc_output__cat_charp(os, WC_REPLACE);
            return;
        default:
            if (opts.ucs_conv)
                cc = wc_any_to_any_ces(opts, cc, st);
            else
                cc.ccs = WC_CCS_IS_WIDE(cc.ccs) ? WC_CCS_UNKNOWN_W : WC_CCS_UNKNOWN;
            continue;
        }
    }
}

void wc_push_to_hz_end(struct wc_output* os, struct wc_status* st)
{
    if (st->gl) {
        wc_output__cat_char(os, WC_C_HZ_TILDA);
        wc_output__cat_char(os, WC_C_HZ_SO);
        st->gl = 0;
    }
}
