#pragma once
#include <stdint.h>
#include "ces.h"

extern uint8_t WC_DETECT_MAP[];

extern void wc_create_detect_map(wc_ces ces, bool esc);
struct wc_option;
extern wc_ces wc_auto_detect(struct wc_option *WcOption, char* is, size_t len, wc_ces hint);

