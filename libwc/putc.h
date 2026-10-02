#pragma once
#include "ces.h"

struct wc_option;
extern void wc_putc_init(struct wc_option* WcOption, wc_ces f_ces, wc_ces t_ces);
extern void wc_putc(struct wc_option* WcOption, const char* c, FILE* f);
extern void wc_putc_end(struct wc_option *WcOption, FILE* f);
extern void wc_putc_clear_status(void);
