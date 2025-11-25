#pragma once
#include <stddef.h>

extern int LINES, COLS;
#if defined(__CYGWIN__)
extern int LASTLINE;
#endif

/* Addition:mouse event */
#define MOUSE_BTN1_DOWN 0
#define MOUSE_BTN2_DOWN 1
#define MOUSE_BTN3_DOWN 2
#define MOUSE_BTN4_DOWN_RXVT 3
#define MOUSE_BTN5_DOWN_RXVT 4
#define MOUSE_BTN4_DOWN_XTERM 64
#define MOUSE_BTN5_DOWN_XTERM 65
#define MOUSE_BTN_UP 3
#define MOUSE_BTN_RESET -1

extern void put_image_osc5379(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
extern void put_image_sixel(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
extern void put_image_iterm2(char* url, int x, int y, int w, int h);
extern void put_image_kitty(char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
extern int get_pixel_per_cell(int* ppc, int* ppl);

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
void addch(char c);
void wrap(void);
int graph_ok(void);
void refresh(void);
void clear(void);
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
