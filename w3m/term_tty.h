#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

void tty_linescols(void);
void tty_clear(void);
void ttymode_add(int mode, int imode);
void ttymode_remove(int mode, int imode);
void tty_crmode(void);
void tty_echo(void);
void tty_noecho(void);
void tty_raw(void);
void tty_cbreak(void);
void tty_bell(void);

void tty_flush(void);
bool tty_pixel_per_cell(int* ppc, int* ppl);
void tty_write(const uint8_t* str, size_t len);
static inline void tty_write_str(const char* str)
{
    tty_write((const uint8_t*)str, strlen(str));
}
