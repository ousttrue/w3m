#pragma once
#include <stddef.h>

extern int LINES, COLS;

struct TermSize {
    int lines;
    int cols;
};

void setlinescols(int lines, int cols);
struct TermSize get_term_size();

int tty_get_pixel_per_cell(int* ppc, int* ppl);

void ttymode_set(int mode, int imode);
void ttymode_reset(int mode, int imode);
char getch(void);
void tty_write(const char* s);
void tty_move(int line, int column);
char* tty_name(void);
void reset_tty(void);
void set_int(void);
int initscr(void);
void wrap(void);
int graph_ok(void);
void tty_render_screen(void);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_cbreak(void);
void tty_set_title(const char* s);
void tty_flush(void);
void tty_bell();
int tty_sleep_till_anykey(int sec, int purge);
