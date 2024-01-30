#pragma once

extern int LINES, COLS;
#define LASTLINE (LINES - 1)

#define GRAPHIC_CHAR_ASCII 2
#define GRAPHIC_CHAR_DEC 1
#define GRAPHIC_CHAR_CHARSET 0

extern bool Do_not_use_ti_te;
extern char *displayTitleTerm;
extern char UseGraphicChar;

char getch(void);
int set_tty(void);
void set_cc(int spec, int val);
void close_tty(void);
char *ttyname_tty(void);
void reset_tty(void);
void set_int(void);
void getTCstr(void);
void setlinescols(void);
void setupscreen(void);
int initscr(void);
void move(int line, int column);
void addch(char c);
void wrap(void);
void touch_line(void);
void standout(void);
void standend(void);
void bold(void);
void boldend(void);
void underline(void);
void underlineend(void);
void graphstart(void);
void graphend(void);
int graph_ok(void);
void refresh(void);
void clear(void);
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(char *s);
void addnstr(char *s, int n);
void addnstr_sup(char *s, int n);
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
void term_cbreak(void);
void term_title(char *s);
void flush_tty(void);
void toggle_stand(void);
void bell(void);
int sleep_till_anykey(int sec, int purge);
void initMimeTypes(void);
void free_ssl_ctx(void);
