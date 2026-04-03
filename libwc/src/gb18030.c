#include "gb18030.h"
#include "conv.h"
#include "status.h"
#include "ccs.h"
#include "search.h"
#include "wtf.h"
#include "ucs.h"
#include "map/gb18030_ucs.map"

#define C0 WC_GB18030_MAP_C0
#define GL WC_GB18030_MAP_GL
#define C1 WC_GB18030_MAP_C1
#define LB WC_GB18030_MAP_LB
#define UB WC_GB18030_MAP_UB
#define L4 WC_GB18030_MAP_L4

// clang-format off
wc_uint8 WC_GB18030_MAP[ 0x100 ] = {
    C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0,
    C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0,
    GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL,
    L4, L4, L4, L4, L4, L4, L4, L4, L4, L4, GL, GL, GL, GL, GL, GL,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, C0,

    LB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, C1,
};
// clang-format on

struct wc_wchar
wc_gbk_ext_to_cs128w(struct wc_wchar cc)
{
    cc.code = WC_GBK_N(cc.code);
    if (cc.code < 0x4000)
        cc.ccs = WC_CCS_GBK_EXT_1;
    else {
        cc.ccs = WC_CCS_GBK_EXT_2;
        cc.code -= 0x4000;
    }
    cc.code = WC_N_CS128W(cc.code);
    return cc;
}

struct wc_wchar
wc_cs128w_to_gbk_ext(struct wc_wchar cc)
{
    cc.code = WC_CS128W_N(cc.code);
    if (cc.ccs == WC_CCS_GBK_EXT_2)
        cc.code += 0x4000;
    cc.ccs = WC_CCS_GBK_EXT;
    cc.code = WC_N_GBK(cc.code);
    return cc;
}

static wc_ccs
wc_gbk_or_gbk_ext(wc_uint16 code)
{
    return wc_map3_range_search(code,
               gbk_ext_ucs_map, N_gbk_ext_ucs_map)
        ? WC_CCS_GBK_EXT
        : WC_CCS_GBK;
}

#ifdef USE_UNICODE
wc_uint32
wc_gb18030_to_ucs(struct wc_option opts, struct wc_wchar cc)
{
    struct wc_map3* map;

    switch (WC_CCS_SET(cc.ccs)) {
    case WC_CCS_GBK_EXT_1:
    case WC_CCS_GBK_EXT_2:
        cc = wc_cs128w_to_gbk_ext(cc);
    case WC_CCS_GBK_EXT:
        map = wc_map3_range_search((wc_uint16)cc.code,
            gbk_ext_ucs_map, N_gbk_ext_ucs_map);
        if (map)
            return map->code3 + WC_GBK_N(cc.code) - WC_GBK_N(map->code2);
        return WC_C_UCS4_ERROR;
    case WC_CCS_GB18030:
        break;
    default:
        return wc_any_to_ucs(opts, cc);
    }
    if (cc.code >= WC_C_GB18030_UCS2 && cc.code <= WC_C_GB18030_UCS2_END) {
        int i, min = 0, max = N_ucs_gb18030_map - 1;

        cc.code = WC_GB18030_N(cc.code) - WC_GB18030_N(WC_C_GB18030_UCS2);
        if (cc.code >= ucs_gb18030_map[max].code3)
            i = max;
        else {
            while (1) {
                i = (min + max) / 2;
                if (min == max)
                    break;
                if (cc.code < ucs_gb18030_map[i].code3)
                    max = i - 1;
                else if (cc.code >= ucs_gb18030_map[i + 1].code3)
                    min = i + 1;
                else
                    break;
            }
        }
        return ucs_gb18030_map[i].code + cc.code - ucs_gb18030_map[i].code3;
    }
    if (cc.code >= WC_C_GB18030_UCS4 && cc.code <= WC_C_GB18030_UCS4_END)
        return WC_GB18030_N(cc.code) - WC_GB18030_N(WC_C_GB18030_UCS4)
            + 0x10000;
    return WC_C_UCS4_ERROR;
}

struct wc_wchar
wc_ucs_to_gb18030(struct wc_option opts, wc_uint32 ucs)
{
    struct wc_wchar cc;
    struct wc_map3* map;

    if (ucs <= WC_C_UCS2_END) {
        map = wc_map3_range_search((wc_uint16)ucs,
            ucs_gbk_ext_map, N_ucs_gbk_ext_map);
        if (map) {
            cc.code = WC_GBK_N(map->code3) + ucs - map->code;
            cc.code = WC_N_GBK(cc.code);
            cc.ccs = WC_CCS_GBK_EXT;
            return cc;
        }
        map = wc_map3_range_search((wc_uint16)ucs,
            ucs_gb18030_map, N_ucs_gb18030_map);
        if (map) {
            cc.code = map->code3 + ucs - map->code + WC_GB18030_N(WC_C_GB18030_UCS2);
            cc.code = WC_N_GB18030(cc.code);
            if (opts.gb18030_as_ucs)
                cc.ccs = WC_CCS_GB18030 | (wc_ucs_to_ccs(opts, ucs) & ~WC_CCS_A_SET);
            else
                cc.ccs = WC_CCS_GB18030_W;
            return cc;
        }
    } else if (ucs <= WC_C_UNICODE_END) {
        cc.code = ucs - 0x10000 + WC_GB18030_N(WC_C_GB18030_UCS4);
        cc.code = WC_N_GB18030(cc.code);
        if (opts.gb18030_as_ucs)
            cc.ccs = WC_CCS_GB18030 | (wc_ucs_to_ccs(opts, ucs) & ~WC_CCS_A_SET);
        else
            cc.ccs = WC_CCS_GB18030_W;
        return cc;
    }
    cc.ccs = WC_CCS_UNKNOWN;
    cc.code = 0;
    return cc;
}
#endif

struct wc_output wc_conv_from_gb18030(struct wc_option opts, const char* is, int len, wc_ces ces)
{
    wc_uchar* sp = (wc_uchar*)is;
    wc_uchar* ep = sp + len;
    wc_uchar* p;
    int state = WC_GB18030_NOSTATE;
    wc_uint32 gbk;
    struct wc_wchar cc;
    wc_uint32 ucs;

    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep)
        return wc_output_from_charp(is);
    struct wc_output os = wc_output_from_size(len);
    if (p > sp)
        wc_output__cat_charp_n(&os, is, (int)(p - sp));

    for (; p < ep; p++) {
        switch (state) {
        case WC_GB18030_NOSTATE:
            switch (WC_GB18030_MAP[*p]) {
            case UB:
                state = WC_GB18030_MBYTE1;
                break;
            case C1:
                wtf_push_unknown(opts, &os, p, 1);
                break;
            default:
                wc_output__cat_char(&os, (char)*p);
                break;
            }
            break;
        case WC_GB18030_MBYTE1:
            if (WC_GB18030_MAP[*p] & LB) {
                gbk = ((wc_uint32) * (p - 1) << 8) | *p;
                if (wc_gbk_or_gbk_ext(gbk) == WC_CCS_GBK_EXT)
                    wtf_push(opts, &os, WC_CCS_GBK_EXT, gbk);
                else if (*(p - 1) >= 0xA1 && *p >= 0xA1)
                    wtf_push(opts, &os, wc_gb2312_or_gbk(gbk), gbk);
                else
                    wtf_push(opts, &os, WC_CCS_GBK, gbk);
            } else if (WC_GB18030_MAP[*p] == L4) {
                state = WC_GB18030_MBYTE2;
                break;
            } else
                wtf_push_unknown(opts, &os, p - 1, 2);
            state = WC_GB18030_NOSTATE;
            break;
        case WC_GB18030_MBYTE2:
            if (WC_GB18030_MAP[*p] == UB) {
                state = WC_GB18030_MBYTE3;
                break;
            } else
                wtf_push_unknown(opts, &os, p - 2, 3);
            state = WC_GB18030_NOSTATE;
            break;
        case WC_GB18030_MBYTE3:
            if (WC_GB18030_MAP[*p] == L4) {
                cc.ccs = WC_CCS_GB18030_W;
                cc.code = ((wc_uint32) * (p - 3) << 24)
                    | ((wc_uint32) * (p - 2) << 16)
                    | ((wc_uint32) * (p - 1) << 8)
                    | *p;
                if (opts.gb18030_as_ucs && (ucs = wc_gb18030_to_ucs(opts, cc)) != WC_C_UCS4_ERROR)
                    wtf_push(opts, &os, WC_CCS_GB18030 | (wc_ucs_to_ccs(opts, ucs) & ~WC_CCS_A_SET), cc.code);
                else
                    wtf_push(opts, &os, cc.ccs, cc.code);
            } else
                wtf_push_unknown(opts, &os, p - 3, 4);
            state = WC_GB18030_NOSTATE;
            break;
        }
    }
    switch (state) {
    case WC_GB18030_MBYTE1:
        wtf_push_unknown(opts, &os, p - 1, 1);
        break;
    case WC_GB18030_MBYTE2:
        wtf_push_unknown(opts, &os, p - 2, 2);
        break;
    case WC_GB18030_MBYTE3:
        wtf_push_unknown(opts, &os, p - 3, 3);
        break;
    }
    return os;
}

void wc_push_to_gb18030(struct wc_option opts, struct wc_output* os, struct wc_wchar cc, struct wc_status* st)
{
    while (1) {
        switch (WC_CCS_SET(cc.ccs)) {
        case WC_CCS_US_ASCII:
            wc_output__cat_char(os, (char)cc.code);
            return;
        case WC_CCS_GB_2312:
            wc_output__cat_char(os, (char)((cc.code >> 8) | 0x80));
            wc_output__cat_char(os, (char)((cc.code & 0xff) | 0x80));
            return;
        case WC_CCS_GBK_1:
        case WC_CCS_GBK_2:
            cc = wc_cs128w_to_gbk(cc);
        case WC_CCS_GBK:
            wc_output__cat_char(os, (char)(cc.code >> 8));
            wc_output__cat_char(os, (char)(cc.code & 0xff));
            return;
        case WC_CCS_GBK_EXT_1:
        case WC_CCS_GBK_EXT_2:
            cc = wc_cs128w_to_gbk(cc);
        case WC_CCS_GBK_EXT:
            wc_output__cat_char(os, (char)(cc.code >> 8));
            wc_output__cat_char(os, (char)(cc.code & 0xff));
            return;
        case WC_CCS_GB18030:
            wc_output__cat_char(os, (char)((cc.code >> 24) & 0xff));
            wc_output__cat_char(os, (char)((cc.code >> 16) & 0xff));
            wc_output__cat_char(os, (char)((cc.code >> 8) & 0xff));
            wc_output__cat_char(os, (char)(cc.code & 0xff));
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

struct wc_output wc_char_conv_from_gb18030(struct wc_option opts, wc_uchar c, struct wc_status* st)
{
    static struct wc_output os;
    static wc_uchar gb[4];
    wc_uint32 gbk;
    struct wc_wchar cc;
    wc_uint32 ucs;

    if (st->state == -1) {
        st->state = WC_GB18030_NOSTATE;
        os = wc_output_from_size(8);
    }

    switch (st->state) {
    case WC_GB18030_NOSTATE:
        switch (WC_GB18030_MAP[c]) {
        case UB:
            gb[0] = c;
            st->state = WC_GB18030_MBYTE1;
            return (struct wc_output) { 0 };
        case C1:
            break;
        default:
            wc_output__cat_char(&os, (char)c);
            break;
        }
        break;
    case WC_GB18030_MBYTE1:
        if (WC_GB18030_MAP[c] & LB) {
            gbk = ((wc_uint32)gb[0] << 8) | c;
            if (wc_gbk_or_gbk_ext(gbk) == WC_CCS_GBK_EXT)
                wtf_push(opts, &os, WC_CCS_GBK_EXT, gbk);
            else if (gb[0] >= 0xA1 && c >= 0xA1)
                wtf_push(opts, &os, wc_gb2312_or_gbk(gbk), gbk);
            else
                wtf_push(opts, &os, WC_CCS_GBK, gbk);
        } else if (WC_GB18030_MAP[c] == L4) {
            gb[1] = c;
            st->state = WC_GB18030_MBYTE2;
            return (struct wc_output) { 0 };
        }
        break;
    case WC_GB18030_MBYTE2:
        if (WC_GB18030_MAP[c] == UB) {
            gb[2] = c;
            st->state = WC_GB18030_MBYTE3;
            return (struct wc_output) { 0 };
        }
        break;
    case WC_GB18030_MBYTE3:
        if (WC_GB18030_MAP[c] == L4) {
            cc.ccs = WC_CCS_GB18030_W;
            cc.code = ((wc_uint32)gb[0] << 24)
                | ((wc_uint32)gb[1] << 16)
                | ((wc_uint32)gb[2] << 8)
                | c;

            if (opts.gb18030_as_ucs && (ucs = wc_gb18030_to_ucs(opts, cc)) != WC_C_UCS4_ERROR)
                wtf_push(opts, &os, WC_CCS_GB18030 | (wc_ucs_to_ccs(opts, ucs) & ~WC_CCS_A_SET), cc.code);
            else
                wtf_push(opts, &os, cc.ccs, cc.code);
        }
        break;
    }
    st->state = -1;
    return os;
}
