#pragma once
#include "ces.h"

extern void wc_putc_init(wc_ces f_ces, wc_ces t_ces);
extern void wc_putc(const char* c, FILE* f);
extern void wc_putc_end(FILE* f);
extern void wc_putc_clear_status(void);

