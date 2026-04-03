#pragma once
#include "wc_types.h"

void wc_putc_init(struct wc_option opts, wc_ces f_ces, wc_ces t_ces);

typedef void (*WriterFunc)(const wc_uchar* str, size_t len);
void wc_putc(struct wc_option opts, char* c, WriterFunc f);
void wc_putc_end(WriterFunc f);
void wc_putc_clear_status(void);
