#pragma once
#include "status.h"

extern uint8_t WC_DETECT_MAP[];

extern void wc_create_detect_map(wc_ces ces, bool esc);
extern wc_ces wc_auto_detect(char* is, size_t len, wc_ces hint);

