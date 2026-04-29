#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

int set_tty(void);
void setlinescols(void);
void tty_clear(void);
void set_cc(int spec, int val);
void ttymode_add(int mode, int imode);
void ttymode_remove(int mode, int imode);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_cbreak(void);
void bell(void);

void tty_flush(void);
bool get_pixel_per_cell(int* ppc, int* ppl);
void writer(const uint8_t* str, size_t len);
int write1(int c);
