#pragma once
#include <stdio.h>

extern const char* displayTitleTerm;
extern int opt_cols;
extern int use_ti_te;

extern char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
    *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
    *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;
extern char gcmap[96];

typedef void (*SigActionFunc)(int);
void TrapOn(SigActionFunc keyAbort, volatile SigActionFunc *prevtrap);
void TrapOff();

#define TRAP_ON TrapOn(KeyAbort, &prevtrap)
#define TRAP_OFF TrapOff()

struct TermSize {
    int lines;
    int cols;
};
struct TermSize termSize();

#define COLS termSize().cols
#define LINES termSize().lines
#define LASTLINE (termSize().lines - 1)

int tty_init(void);
const char* tty_name(void);
void tty_update_size(void);
FILE* tty_file(void);
extern FILE* ttyf;

// mode
void ttymode_set(int mode, int imode);
void ttymode_reset(int mode, int imode);
void tty_reset(void);
void tty_crmode(void);
void tty_noecho(void);
void tty_raw(void);
void tty_cooked(void);
void tty_cbreak(void);

// in
int tty_sleep_till_anykey(int sec, int purge);
char tty_getch(void);

// out
void tty_flush(void);
void tty_title(const char* s);
void tty_bell(void);

// image
int get_pixel_per_cell(int* ppc, int* ppl);
void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(const char* url, int x, int y, int w, int h);
void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
