#pragma once

extern int LINES, COLS;
#define LASTLINE (LINES - 1)

void term_fmTerm();
void term_fmInit();
void term_setlinescols();
bool term_graph_ok();
void term_clear();
void term_title(const char *s);
void term_bell();
void term_refresh();

extern int tgetent(char *, char *);
extern int tgetnum(char *);
extern int tgetflag(char *);
extern char *tgetstr(char *, char **);
extern char *tgoto(const char *, int, int);
extern int tputs(const char *, int, int (*)(char));
