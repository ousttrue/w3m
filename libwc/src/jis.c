#include "jis.h"
#include "ccs.h"
#include "search.h"
#include "ucs.h"

#include "map/jisx0201k_jisx0208.map"
#include "map/jisx0208_jisx02131.map"

struct wc_wchar
wc_jisx0201k_to_jisx0208(struct wc_wchar cc)
{
    cc.code = jisx0201k_jisx0208_map[cc.code & 0x7f];
    cc.ccs = cc.code ? WC_CCS_JIS_X_0208 : WC_CCS_UNKNOWN_W;
    return cc;
}

struct wc_wchar
wc_jisx0212_to_jisx0213(struct wc_option opts, struct wc_wchar cc)
{
    struct wc_wchar cc2;
    static struct wc_table* t1 = NULL;
    static struct wc_table* t2 = NULL;

    if (t1 == NULL) {
        t1 = wc_get_ucs_table(WC_CCS_JIS_X_0213_1);
        t2 = wc_get_ucs_table(WC_CCS_JIS_X_0213_2);
    }
    cc2 = wc_any_to_any(opts, cc, t2);
    if (cc2.ccs == WC_CCS_JIS_X_0212)
        return cc2;
    return wc_any_to_any(opts, cc, t1);
}

struct wc_wchar
wc_jisx0213_to_jisx0212(struct wc_option opts, struct wc_wchar cc)
{
    static struct wc_table* t = NULL;

    if (t == NULL)
        t = wc_get_ucs_table(WC_CCS_JIS_X_0212);
    return wc_any_to_any(opts, cc, t);
}

wc_ccs
wc_jisx0208_or_jisx02131(wc_uint16 code)
{
    return wc_map_range_search(code & 0x7f7f,
               jisx0208_jisx02131_map, N_jisx0208_jisx02131_map)
        ? WC_CCS_JIS_X_0213_1
        : WC_CCS_JIS_X_0208;
}

wc_ccs
wc_jisx0212_or_jisx02132(wc_uint16 code)
{
    return wc_jisx0212_jisx02132_map[(code >> 8) & 0x7f]
        ? WC_CCS_JIS_X_0213_2
        : WC_CCS_JIS_X_0212;
}
