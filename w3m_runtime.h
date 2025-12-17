#pragma once
/// process
/// tty
/// signal
#include <stdbool.h>

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        if (fmInitialized())                   \
            term_cbreak();                     \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        if (fmInitialized())            \
            term_raw();                 \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }

#include <stdio.h>
struct Runtime {
    int tty_input;
    FILE* tty_output_f;
    int lines;
    int cols;

    char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
        *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
        *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;
    char gcmap[96];

    bool fmInitialized;
    int Do_not_use_ti_te;
};
struct Runtime* getRuntime(void);

void reset_error_exit(int);
char graphchar(char c);
void writestr(const char* s);
int write1(int c);
bool fmInitialized(void);
void init_tty();
void flush_tty(void);
void reset_tty(void);
void ttymode_set(int mode, int imode);
void ttymode_reset(int mode, int imode);
void set_cc(int spec, int val);
void w3m_exit(int i);
char* ttyname_tty(void);
int initscr(void);
void tty_MOVE(int line, int column);
void (*mySignal(int signal_number, void (*action)(int)))(int);

inline static int TTY_LINES(void) { return getRuntime()->lines; }
inline static int TTY_COLS(void) { return getRuntime()->cols; }
inline static int LASTLINE(void) { return getRuntime()->lines - 1; }
void tty_set_cols(int cols);
int graph_ok(void);
