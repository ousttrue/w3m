
#include <string.h>
#include "../alloc.h"

#include "wc.h"

wc_option WcOption = {
    WC_OPT_DETECT_ON, /* auto_detect */
    WC_TRUE,          /* use_combining */
    WC_TRUE,          /* use_language_tag */
    WC_TRUE,          /* ucs_conv */
    WC_FALSE,         /* pre_conv */
    WC_TRUE,          /* fix_width_conv */
    WC_FALSE,         /* use_gb12345_map */
    WC_FALSE,         /* use_jisx0201 */
    WC_FALSE,         /* use_jisc6226 */
    WC_FALSE,         /* use_jisx0201k */
    WC_FALSE,         /* use_jisx0212 */
    WC_FALSE,         /* use_jisx0213 */
    WC_TRUE,          /* strict_iso2022 */
    WC_FALSE,         /* gb18030_as_ucs */
    WC_FALSE,         /* no_replace */
    WC_TRUE,          /* use_wide */
    WC_FALSE,         /* east_asian_width */
};

static wc_status output_st;
static wc_option output_option;
static wc_bool output_set = WC_FALSE;

#define wc_option_cmp(opt1, opt2)                                              \
  memcmp((void *)(opt1), (void *)(opt2), sizeof(wc_option))

void wc_input_init(wc_ces ces, wc_status *st) {
  wc_gset *gset;
  int i, g;

  st->ces_info = &WcCesInfo[WC_CES_INDEX(ces)];
  gset = st->ces_info->gset;

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
}

void wc_output_init(wc_ces ces, wc_status *st) {
  wc_gset *gset;

  if (output_set && ces == output_st.ces_info->id &&
      !wc_option_cmp(&WcOption, &output_option)) {
    *st = output_st;
    return;
  }

  st->state = 0;
  st->ces_info = &WcCesInfo[WC_CES_INDEX(ces)];
  gset = st->ces_info->gset;

  st->g0_ccs = ((ces == WC_CES_ISO_2022_JP || ces == WC_CES_ISO_2022_JP_2 ||
                 ces == WC_CES_ISO_2022_JP_3) &&
                WcOption.use_jisx0201)
                   ? WC_CCS_JIS_X_0201
                   : gset[0].ccs;
  st->g1_ccs = ((ces == WC_CES_ISO_2022_JP || ces == WC_CES_ISO_2022_JP_2 ||
                 ces == WC_CES_ISO_2022_JP_3) &&
                WcOption.use_jisc6226)
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
    wc_create_gmap(st);

  output_st = *st;
  output_set = WC_TRUE;
  output_option = WcOption;
}

wc_bool wc_ces_has_ccs(wc_ccs ccs, wc_status *st) {
  wc_gset *gset = st->ces_info->gset;
  int i;

  for (i = 0; gset[i].ccs; i++) {
    if (ccs == gset[i].ccs)
      return WC_TRUE;
  }
  return WC_FALSE;
}
