#pragma once
#include "utf8.h"

void tty_open();
void tty_close();

void tty_cbreak();
void tty_crmode();
void tty_nocrmode();
void tty_echo();
void tty_noecho();
void tty_raw();
void tty_cooked();

void tty_flush();
int tty_putc(int c);
int tty_put_utf8(struct Utf8 utf8);
void tty_printf(const char *fmt, ...);

char tty_getch();
int tty_sleep_till_anykey(int sec, bool purge);
