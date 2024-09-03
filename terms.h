#pragma once

extern int LINES, COLS;
void setlinescols();

extern int tgetent(char *, char *);
extern int tgetnum(char *);
extern int tgetflag(char *);
extern char *tgetstr(char *, char **);
extern char *tgoto(char *, int, int);
extern int tputs(char *, int, int (*)(char));

// escape sequence
void standout();
void standend();
void bold();
void boldend();
void underline();
void underlineend();
// graph
int graph_ok();
void graphend();
void graphstart();

// tty
void flush_tty();
int set_tty();
void close_tty();
char getch();
int sleep_till_anykey(int sec, int purge);
void term_cbreak();
void term_title(const char *s);
void bell();

// screen
void refresh();
void clear();
void move(int line, int column);
void addch(char c);
void touch_line();
void touch_column(int);
void wrap();
void clrtoeol(); /* conflicts with curs_clear(3)? */
int initscr(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void addstr(char *s);
void addnstr(char *s, int n);
void addnstr_sup(char *s, int n);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void toggle_stand(void);
