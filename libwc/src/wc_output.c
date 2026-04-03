#include "wc_output.h"
#include "status.h"
#include "ucs.h"

#include "hz.h"
#include "utf8.h"
#include "utf7.h"
#include <stdlib.h>
#include <string.h>

// static struct wc_status output_st;
// static struct wc_option output_option;
// static wc_bool output_set = WC_FALSE;

// #define wc_option_cmp(opt1, opt2) \
//     memcmp((void*)(opt1), (void*)(opt2), sizeof(struct wc_option))

void wc_output_init(struct wc_option opts, wc_ces ces, struct wc_status* st)
{
    size_t i, n, nw;

    // if (output_set && ces == output_st.ces_info->id && 0 == memcpy(opts, &output_option, sizeof(struct wc_option))) {
    //     *st = output_st;
    //     return;
    // }

    st->state = 0;
    st->ces_info = &WcCesInfo[WC_CES_INDEX(ces)];
    const struct wc_gset* gset = st->ces_info->gset;

    st->g0_ccs = ((ces == WC_CES_ISO_2022_JP || ces == WC_CES_ISO_2022_JP_2 || ces == WC_CES_ISO_2022_JP_3) && opts.use_jisx0201)
        ? WC_CCS_JIS_X_0201
        : gset[0].ccs;
    st->g1_ccs = ((ces == WC_CES_ISO_2022_JP || ces == WC_CES_ISO_2022_JP_2 || ces == WC_CES_ISO_2022_JP_3) && opts.use_jisc6226)
        ? WC_CCS_JIS_C_6226
        : gset[1].ccs;
    st->design[0] = st->g0_ccs;
    st->design[1] = 0;
    st->design[2] = 0;
    st->design[3] = 0;
    st->gl = 0;
    st->gr = 0;
    st->ss = 0;

    if (ces & WC_CES_T_ISO_2022)
        wc_create_gmap(opts, st);

    st->tag = (struct wc_output) { 0 };
    st->ntag = 0;

    if (!opts.ucs_conv) {
        st->tlist = NULL;
        st->tlistw = NULL;
    } else {

        for (i = n = nw = 0; gset[i].ccs; i++) {
            if (WC_CCS_IS_WIDE(gset[i].ccs))
                nw++;
            else
                n++;
        }
        st->tlist = malloc(sizeof(struct wc_table*) * (n + 1));
        st->tlistw = malloc(sizeof(struct wc_table*) * (nw + 1));
        for (i = n = nw = 0; gset[i].ccs; i++) {
            if (WC_CCS_IS_WIDE(gset[i].ccs)) {
                switch (gset[i].ccs) {
                case WC_CCS_JIS_X_0212:
                    if (!opts.use_jisx0212)
                        continue;
                    break;
                case WC_CCS_JIS_X_0213_1:
                case WC_CCS_JIS_X_0213_2:
                    if (!opts.use_jisx0213)
                        continue;
                    break;
                case WC_CCS_GB_2312:
                    if (opts.use_gb12345_map && ces != WC_CES_GBK && ces != WC_CES_GB18030) {
                        st->tlistw[nw++] = wc_get_ucs_table(WC_CCS_GB_12345);
                        continue;
                    }
                    break;
                }
                st->tlistw[nw++] = wc_get_ucs_table(gset[i].ccs);
            } else {
                switch (gset[i].ccs) {
                case WC_CCS_JIS_X_0201K:
                    if (!opts.use_jisx0201k)
                        continue;
                    break;
                }
                st->tlist[n++] = wc_get_ucs_table(gset[i].ccs);
            }
        }
        st->tlist[n] = NULL;
        st->tlistw[nw] = NULL;
    }
}

void wc_push_end(struct wc_output* os, struct wc_status* st)
{
    if (st->ces_info->id & WC_CES_T_ISO_2022)
        wc_push_to_iso2022_end(os, st);
    else if (st->ces_info->id == WC_CES_HZ_GB_2312)
        wc_push_to_hz_end(os, st);
    else if (st->ces_info->id == WC_CES_UTF_8)
        wc_push_to_utf8_end(os, st);
    else if (st->ces_info->id == WC_CES_UTF_7)
        wc_push_to_utf7_end(os, st);
}
