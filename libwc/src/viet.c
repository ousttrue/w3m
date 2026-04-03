#include "viet.h"
#include "conv.h"
#include "status.h"
#include "detect.h"
#include "ces.h"
#include "ccs.h"
#include "wtf.h"
#include "search.h"
#include "ucs.h"
#include "map/tcvn57123_tcvn5712.map"

// clang-format off
wc_uint8 wc_c0_tcvn57122_map[ 0x20 ] = {
    0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,     
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,     
};
wc_uint8 wc_c0_viscii112_map[ 0x20 ] = {
    0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,     
    0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,     
};
wc_uint8 wc_c0_vps2_map[ 0x20 ] = {
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,     
    1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,     
};
static wc_uint8 tcvn5712_precompose_map[ 0x100 ] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*     A           E           I                 O */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 U           Y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
/*     a           e           i                 o */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 u           y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*     A( A^ E^ O^ O+ U+    a( a^ e^ o^ o+ u+      */
    0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0,
/*  `  ?  ~  '  .                                  */
    2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static wc_uint8 cp1258_precompose_map[ 0x100 ] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*     A           E           I                 O */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 U           Y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
/*     a           e           i                 o */
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
/*                 u           y                   */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/*        A^ A(                   E^    `          */
    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0,
/*        ?     O^ O+                      U+ ~    */
    0, 0, 2, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0,
/*        a^ a(                   e^    '          */
    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0,
/*        .     o^ o+                      u+      */
    0, 0, 2, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
};
// clang-format on

wc_uint32
wc_tcvn5712_precompose(wc_uchar c1, wc_uchar c2)
{
    if (tcvn5712_precompose_map[c1] == 1 && tcvn5712_precompose_map[c2] == 2)
        return ((wc_uint32)c1 << 8) | c2;
    else
        return 0;
}

struct wc_wchar
wc_tcvn57123_to_tcvn5712(struct wc_wchar cc)
{
    struct wc_map* map;

    map = wc_map_search((wc_uint16)(cc.code & 0x7f7f),
        tcvn57123_tcvn5712_map, N_tcvn57123_tcvn5712_map);
    if (map) {
        cc.ccs = (map->code2 < 0x20) ? WC_CCS_TCVN_5712_2 : WC_CCS_TCVN_5712_1;
        cc.code = map->code2 | 0x80;
    } else {
        cc.ccs = WC_CCS_UNKNOWN;
    }
    return cc;
}

wc_uint32
wc_cp1258_precompose(wc_uchar c1, wc_uchar c2)
{
    if (cp1258_precompose_map[c1] == 1 && cp1258_precompose_map[c2] == 2)
        return ((wc_uint32)c1 << 8) | c2;
    else
        return 0;
}

struct wc_output wc_conv_from_viet(struct wc_option opts, const char* is, int len, wc_ces ces)
{
    wc_uchar* sp = (wc_uchar*)is;
    wc_uchar* ep = sp + len;
    wc_uchar* p;
    wc_ccs ccs1 = WcCesInfo[WC_CCS_INDEX(ces)].gset[1].ccs;
    wc_ccs ccs2 = WcCesInfo[WC_CCS_INDEX(ces)].gset[2].ccs;
    wc_uint8* map = NULL;

    switch (ces) {
    case WC_CES_TCVN_5712:
        map = wc_c0_tcvn57122_map;
        break;
    case WC_CES_VISCII_11:
        map = wc_c0_viscii112_map;
        break;
    case WC_CES_VPS:
        map = wc_c0_vps2_map;
        break;
    }

    wc_create_detect_map(ces, WC_FALSE);
    for (p = sp; p < ep && !WC_DETECT_MAP[*p]; p++)
        ;
    if (p == ep)
        return wc_output_from_charp(is);
    struct wc_output os = wc_output_from_size(len);
    if (p > sp)
        wc_output__cat_charp_n(&os, is, (int)(p - sp));

    for (; p < ep; p++) {
        if (*p & 0x80)
            wtf_push(opts, &os, ccs1, (wc_uint32)*p);
        else if (*p < 0x20 && map[*p])
            wtf_push(opts, &os, ccs2, (wc_uint32)*p);
        else
            wc_output__cat_char(&os, (char)*p);
    }
    return os;
}

void wc_push_to_viet(struct wc_option opts, struct wc_output *os, struct wc_wchar cc, struct wc_status* st)
{
    wc_ccs ccs1 = st->ces_info->gset[1].ccs;
    wc_ccs ccs2 = 0, ccs3 = 0;
    wc_uint8* map = NULL;

    switch (st->ces_info->id) {
    case WC_CES_CP1258:
        ccs3 = st->ces_info->gset[2].ccs;
        break;
    case WC_CES_TCVN_5712:
        map = wc_c0_tcvn57122_map;
        ccs2 = st->ces_info->gset[2].ccs;
        ccs3 = st->ces_info->gset[3].ccs;
        break;
    case WC_CES_VISCII_11:
        map = wc_c0_viscii112_map;
        ccs2 = st->ces_info->gset[2].ccs;
        break;
    case WC_CES_VPS:
        map = wc_c0_vps2_map;
        ccs2 = st->ces_info->gset[2].ccs;
        break;
    }

    while (1) {
        if (cc.ccs == ccs1) {
            wc_output__cat_char(os, (char)(cc.code | 0x80));
            return;
        } else if (cc.ccs == ccs2) {
            wc_output__cat_char(os, (char)(cc.code & 0x7f));
            return;
        } else if (cc.ccs == ccs3) {
            wc_output__cat_char(os, (char)((cc.code >> 8) & 0xff));
            wc_output__cat_char(os, (char)(cc.code & 0xff));
            return;
        }
        switch (cc.ccs) {
        case WC_CCS_US_ASCII:
            if (cc.code < 0x20 && map && map[cc.code])
                wc_output__cat_char(os, ' ');
            else
                wc_output__cat_char(os, (char)cc.code);
            return;
        case WC_CCS_UNKNOWN_W:
            if (!opts.no_replace)
                wc_output__cat_charp(os, WC_REPLACE_W);
            return;
        case WC_CCS_UNKNOWN:
            if (!opts.no_replace)
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

struct wc_output wc_char_conv_from_viet(struct wc_option opts, wc_uchar c, struct wc_status* st)
{
    struct wc_output os = wc_output_from_size(1);
    wc_uint8* map = NULL;

    switch (st->ces_info->id) {
    case WC_CES_TCVN_5712:
        map = wc_c0_tcvn57122_map;
        break;
    case WC_CES_VISCII_11:
        map = wc_c0_viscii112_map;
        break;
    case WC_CES_VPS:
        map = wc_c0_vps2_map;
        break;
    }

    if (c & 0x80)
        wtf_push(opts, &os, st->ces_info->gset[1].ccs, (wc_uint32)c);
    else if (c < 0x20 && map[c])
        wtf_push(opts, &os, st->ces_info->gset[2].ccs, (wc_uint32)c);
    else
        wc_output__cat_char(&os, (char)c);
    return os;
}
