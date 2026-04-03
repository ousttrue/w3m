#pragma once
#include "wc_types.h"

extern wc_uint8 WC_DETECT_MAP[];

void wc_create_detect_map(wc_ces ces, wc_bool esc);
wc_ces wc_auto_detect(struct wc_option opts, const char* is, size_t len, wc_ces hint);
