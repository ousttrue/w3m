#pragma once

extern char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
    *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me, *T_ti,
    *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

extern char gcmap[96];

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
int term_putc(char c);
void term_puts(const char *s);

// tty
void flush_tty();
int set_tty();
void close_tty();
char getch();
int sleep_till_anykey(int sec, int purge);
void term_cbreak();
void term_title(const char *s);
void bell();
void crmode(void);
void nocrmode(void);
void term_echo(void);
void term_noecho(void);
void term_raw(void);
void term_cooked(void);
