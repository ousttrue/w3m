/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_TERMS_H
#define W3M_TERMS_H

#include "config.h"

#define DEFAULT_COLS 80
extern int LINES, COLS;
#if defined(__CYGWIN__)
extern int LASTLINE;
#else
#define LASTLINE (LINES-1)
#endif

#define TRAP_ON if (TrapSignal) { \
    prevtrap = mySignal(SIGINT, KeyAbort); \
    if (fmInitialized) \
	term_cbreak(); \
}
#define TRAP_OFF if (TrapSignal) { \
    if (fmInitialized) \
	term_raw(); \
    if (prevtrap) \
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

#ifdef USE_IMAGE
extern void put_image_osc5379(const char *url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
extern void put_image_sixel(const char *url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
extern void put_image_iterm2(const char *url, int x, int y, int w, int h);
extern void put_image_kitty(const char *url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
extern int get_pixel_per_cell(int *ppc, int *ppl);
#endif

char *ttyname_tty(void);
char getch(void);
void reset_tty(void);
void setupscreen(void);

extern char *displayTitleTerm;
extern int use_ti_te;
#endif
