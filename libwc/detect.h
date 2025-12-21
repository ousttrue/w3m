#pragma once
#include "wc_types.h"

extern wc_uint8 WC_DETECT_MAP[];

extern void wc_create_detect_map(wc_ces ces, wc_bool esc);
extern wc_ces wc_auto_detect(char* is, size_t len, wc_ces hint);


