#pragma once
#include "wc_types.h"

#define WC_OPT_DETECT_OFF 0
#define WC_OPT_DETECT_ISO_2022 1
#define WC_OPT_DETECT_ON 2

extern wc_option WcOption;
extern wc_ces_info WcCesInfo[];

extern void wc_input_init(wc_ces ces, wc_status* st);
extern void wc_output_init(wc_ces ces, wc_status* st);
extern void wc_push_end(Str os, wc_status* st);
extern wc_bool wc_ces_has_ccs(wc_ccs ccs, wc_status* st);
