#pragma once
#include <stddef.h>
#include "utf8.h"
#include "termentry.h"

int LINES();
int COLS();
int LASTLINE();

#define GRAPHIC_CHAR_ASCII 2
#define GRAPHIC_CHAR_DEC 1
#define GRAPHIC_CHAR_CHARSET 0

extern bool Do_not_use_ti_te;
extern char *displayTitleTerm;
extern char UseGraphicChar;

FILE *term_io();
const TermEntry &term_entry();
unsigned char term_graphchar(unsigned char c);

int term_write1(char c);
void term_writestr(const char *s);
void term_move(int line, int col);
char getch(void);
void set_cc(int spec, int val);
void close_tty(void);
const char *ttyname_tty(void);
void reset_tty(void);
void set_int(void);
void getTCstr(void);
void setlinescols(void);
int initscr(void);
int graph_ok(void);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_cbreak(void);
void term_title(const char *s);
void flush_tty(void);
void bell(void);
int sleep_till_anykey(int sec, int purge);
