#pragma once
#include <stddef.h>

extern int LINES, COLS;

struct TermSize {
    int lines;
    int cols;
};

void setlinescols(int lines, int cols);
struct TermSize get_term_size();

int get_pixel_per_cell(int* ppc, int* ppl);

void ttymode_set(int mode, int imode);
void ttymode_reset(int mode, int imode);
char getch(void);
void writestr(const char* s);
void MOVE(int line, int column);
int set_tty(void);
void set_cc(int spec, int val);
void close_tty(void);
char* ttyname_tty(void);
void reset_tty(void);
void reset_exit(int);
void error_dump(int);
void set_int(void);
void getTCstr(void);
int initscr(void);
void wrap(void);
int graph_ok(void);
void refresh(void);
#ifdef USE_RAW_SCROLL
void scroll(int);
void rscroll(int);
#endif
void no_clrtoeol(void);
void addstr(char* s);
void addnstr(char* s, int n);
void addnstr_sup(char* s, int n);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_cbreak(void);
void term_title(char* s);
void flush_tty(void);
void bell();
int sleep_till_anykey(int sec, int purge);
