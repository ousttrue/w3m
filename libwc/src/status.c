#include "status.h"
#include "ucs.h"
#include <string.h>

void wc_input_init(wc_ces ces, struct wc_status* st)
{
    int i, g;

    st->ces_info = &WcCesInfo[WC_CES_INDEX(ces)];
    const struct wc_gset* gset = st->ces_info->gset;

    st->state = 0;
    st->g0_ccs = 0;
    st->g1_ccs = 0;
    st->design[0] = gset[0].ccs;
    st->design[1] = gset[1].ccs; /* for ISO-2022-JP/EUC-JP */
    st->design[2] = 0;
    st->design[3] = 0;
    st->gl = 0;
    st->gr = 1;
    st->ss = 0;

    for (i = 0; gset[i].ccs; i++) {
        if (gset[i].init) {
            g = gset[i].g & 0x03;
            if (!st->design[g])
                st->design[g] = gset[i].ccs;
        }
    }

    st->tag = (struct wc_output) { 0 };
    st->ntag = 0;
}

wc_bool
wc_ces_has_ccs(wc_ccs ccs, struct wc_status* st)
{
    const struct wc_gset* gset = st->ces_info->gset;
    int i;

    for (i = 0; gset[i].ccs; i++) {
        if (ccs == gset[i].ccs)
            return WC_TRUE;
    }
    return WC_FALSE;
}
