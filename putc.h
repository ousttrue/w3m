#pragma once
#include "wc_types.h"
#include <Str.h>

struct PutcStatus {
    wc_status putc_st;
    wc_ces putc_f_ces;
    wc_ces putc_t_ces;
    Str putc_str;
};

struct PutcStatus wc_putc_init(wc_ces f_ces, wc_ces t_ces);
void wc_putc(struct PutcStatus* status, char* c, int fd);
void wc_putc_end(struct PutcStatus* status, int fd);
void wc_putc_clear_status(struct PutcStatus* status);
