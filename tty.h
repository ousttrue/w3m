#pragma once

void tty_open();
void tty_close();
int term_putc(char c);
void tty_printf(const char *fmt, ...);
void flush_tty();
char getch();
int sleep_till_anykey(int sec, int purge);
void term_cbreak();

