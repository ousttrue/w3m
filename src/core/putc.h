#pragma once
#include "wc.h"
#include "writer.h"

extern void wc_putc_init(wc_ces f_ces, wc_ces t_ces);
extern void wc_putc(const struct Writer* writer, const char* c);
extern void wc_putc_end(const struct Writer* writer);
extern void wc_putc_clear_status(void);
