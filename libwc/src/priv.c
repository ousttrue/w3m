#include "priv.h"
#include "status.h"
#include "ces.h"
#include "ccs.h"
#include "wtf.h"

struct wc_output wc_conv_from_priv1(struct wc_option opts, const char* is, int len, wc_ces ces)
{
    wc_uchar* sp = (wc_uchar*)is;
    wc_uchar* ep = sp + len;
    wc_uchar* p;
    wc_ccs ccs = WcCesInfo[WC_CCS_INDEX(ces)].gset[1].ccs;

    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep)
        return wc_output_from_charp(is);
    struct wc_output os = wc_output_from_size(len);
    if (p > sp)
        wc_output__cat_charp_n(&os, is, (int)(p - sp));

    for (; p < ep; p++) {
        if (*p & 0x80)
            wtf_push(opts, &os, ccs, (wc_uint32)*p);
        else
            wc_output__cat_char(&os, (char)*p);
    }
    return os;
}

struct wc_output wc_char_conv_from_priv1(struct wc_option opts, wc_uchar c, struct wc_status* st)
{
    struct wc_output os = wc_output_from_size(1);

    if (c & 0x80)
        wtf_push(opts, &os, st->ces_info->gset[1].ccs, (wc_uint32)c);
    else
        wc_output__cat_char(&os, (char)c);
    return os;
}

struct wc_output wc_conv_from_ascii(struct wc_option opts, const char* is, int len, wc_ces ces)
{
    wc_uchar* sp = (wc_uchar*)is;
    wc_uchar* ep = sp + len;
    wc_uchar* p;

    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep)
        return wc_output_from_charp(is);
    struct wc_output os = wc_output_from_size(len);
    if (p > sp)
        wc_output__cat_charp_n(&os, is, (int)(p - sp));

    for (; p < ep; p++) {
        if (*p & 0x80)
            wtf_push_unknown(opts, &os, p, 1);
        else
            wc_output__cat_char(&os, (char)*p);
    }
    return os;
}

void wc_push_to_raw(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st)
{
    switch (cc.ccs) {
    case WC_CCS_US_ASCII:
    case WC_CCS_RAW:
        wc_output__cat_char(os, (char)cc.code);
    }
    return;
}
