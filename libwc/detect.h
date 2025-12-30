#pragma once
#include "ces.h"

extern wc_uint8 WC_DETECT_MAP[];

extern void wc_create_detect_map(enum wc_ces ces, wc_bool esc);
extern enum wc_ces wc_auto_detect(char* is, size_t len, enum wc_ces hint);


