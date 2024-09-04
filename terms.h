#pragma once

extern int LINES, COLS;
#define LASTLINE (LINES - 1)

void setlinescols();
int initscr(void);

extern int tgetent(char *, char *);
extern int tgetnum(char *);
extern int tgetflag(char *);
extern char *tgetstr(char *, char **);
extern char *tgoto(const char *, int, int);
extern int tputs(const char *, int, int (*)(char));

int graph_ok();
void term_clear();
void term_move(int line, int column);
void term_puts(const char *s);
void term_reset();

// tty
void term_title(const char *s);
void bell();
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_refresh();
