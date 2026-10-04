/* vi: set sw=4 ts=8 ai sm noet : */
#pragma once
#include "config.h"

#define DEFAULT_COLS 80

struct TermSize {
    int lines;
    int cols;
};
struct TermSize termSize();

#define COLS termSize().cols
#define LINES termSize().lines
#define LASTLINE (termSize().lines - 1)

#define TRAP_ON                                \
    if (TrapSignal) {                          \
        prevtrap = mySignal(SIGINT, KeyAbort); \
        if (fmInitialized)                     \
            term_cbreak();                     \
    }
#define TRAP_OFF                        \
    if (TrapSignal) {                   \
        if (fmInitialized)              \
            term_raw();                 \
        if (prevtrap)                   \
            mySignal(SIGINT, prevtrap); \
    }

void clear(void);
void flush_tty(void);
void setlinescols(void);

#ifdef USE_MOUSE
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

void mouse_end(void);
#endif

#ifdef __CYGWIN__
#ifdef SUPPORT_WIN9X_CONSOLE_MBCS
extern void enable_win9x_console_input(void);
extern void disable_win9x_console_input(void);
#endif
#endif

extern int get_pixel_per_cell(int* ppc, int* ppl);

char* ttyname_tty(void);
char getch(void);
void reset_tty(void);
void setupscreen(void);

extern char* displayTitleTerm;
extern int use_ti_te;
