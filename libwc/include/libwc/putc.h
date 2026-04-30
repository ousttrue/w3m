#pragma once
#include "wc_types.h"

void wc_putc_init(struct wc_option opts, wc_ces f_ces, wc_ces t_ces);
struct wc_span wc_putc(struct wc_option opts, const char* c);
struct wc_span wc_putc_end();
void wc_putc_clear_status(void);
