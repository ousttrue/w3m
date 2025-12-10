#pragma once
#include <stddef.h>
#include <termios.h>
#include <stdbool.h>

struct TermSize {
    int lines;
    int cols;
};
struct TermSize tty_current_size();
void tty_update_size();

#define LINES tty_current_size().lines
#define COLS tty_current_size().cols

int tty_get_pixel_per_cell(int* ppc, int* ppl);
void tty_immediate_setattr(struct termios* p);

void ttymode_set(int mode, int imode);
void ttymode_reset(int mode, int imode);
char getch(void);
int tty_putc(int c);
void tty_wc_putc_end();
void tty_wc_putc(char* c);
void tty_write(const char* s);
void tty_move(int line, int column);
char* tty_name(void);
void tty_reset(void);
bool initscr(void);
void wrap(void);
void tty_crmode(void);
void tty_nocrmode(void);
void term_noecho(void);
void term_raw(void);
void term_cbreak(void);
void tty_set_title(const char* s);
void tty_flush(void);
void tty_bell();
int tty_sleep_till_anykey(int sec, int purge);
