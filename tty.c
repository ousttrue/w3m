#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "config.h"
#include "tty.h"
#include "ctrlcode.h"
#include "event_poller.h"

//
// input
//
static int g_tty = -1;
int get_tty_fd()
{
    return g_tty;
}

//
// output
//
static FILE* g_ttyf = NULL;
FILE* get_ttyf()
{
    return g_ttyf;
}
void flush_tty(void)
{
    if (g_ttyf) {
        fflush(g_ttyf);
    }
}
int write1(int c)
{
    putc(c, g_ttyf);
    return 0;
}
void writestr(const char* s)
{
    assert(s);
    // tputs(s, 1, &write1);
    for (; *s; ++s) {
        write1(*s);
    }
}

static void _ttyWriterFunc(const char* p, int len, void* user)
{
    fwrite(p, len, 1, g_ttyf);
}
static void _ttyFlushFunc(void* user)
{
    fflush(g_ttyf);
}
static struct Writer g_writer = {
    .user = 0,
    .write = &_ttyWriterFunc,
    .flush = &_ttyFlushFunc,
};
const struct Writer* ttyWriter()
{
    return &g_writer;
}

#ifdef HAVE_TERMIO_H
#include <termio.h>
typedef struct termio TerminalMode;
#define _TerminalSet(fd, x) ioctl(fd, TCSETA, x)
#define _TerminalGet(fd, x) ioctl(fd, TCGETA, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
#endif /* HAVE_TERMIO_H */

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#include <unistd.h>
typedef struct termios TerminalMode;
#define _TerminalSet(fd, x) tcsetattr(fd, TCSANOW, x)
#define _TerminalGet(fd, x) tcgetattr(fd, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
#endif /* HAVE_TERMIOS_H */

#ifdef HAVE_SGTTY_H
#include <sgtty.h>
typedef struct sgttyb TerminalMode;
#define _TerminalSet(fd, x) ioctl(fd, TIOCSETP, x)
#define _TerminalGet(fd, x) ioctl(fd, TIOCGETP, x)
#define MODEFLAG(d) ((d).sg_flags)
#endif /* HAVE_SGTTY_H */

static TerminalMode d_ioval;

void TerminalSet(void* ioval)
{
    _TerminalSet(g_tty, ioval ? ioval : &d_ioval);
}

const char* displayTitleTerm = NULL;

#define W3M_TERM_INFO(name, title, mouse) name, title

static char XTERM_TITLE[] = "\033]0;w3m: %s\007";
static char SCREEN_TITLE[] = "\033k%s\033\134";

/* *INDENT-OFF* */
static struct w3m_term_info {
    char* term;
    char* title_str;
} w3m_term_info_list[] = {
    { W3M_TERM_INFO("xterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("kterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("rxvt", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("Eterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("mlterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("screen", SCREEN_TITLE, 0) },
    { W3M_TERM_INFO(NULL, NULL, 0) }
};
#undef W3M_TERM_INFO
/* *INDENT-ON * */

static char* title_str = NULL;

void set_tty(void)
{
    char* ttyn;
    if (isatty(0)) /* stdin */
        ttyn = ttyname(0);
    else
        ttyn = DEV_TTY_PATH;

    g_tty = open(ttyn, O_RDWR);
    if (g_tty < 0) {
        /* use stderr instead of stdin... is it OK???? */
        g_tty = 2;
    }
    g_ttyf = fdopen(g_tty, "w");
    _TerminalGet(g_tty, &d_ioval);
    if (displayTitleTerm != NULL) {
        struct w3m_term_info* p;
        for (p = w3m_term_info_list; p->term != NULL; p++) {
            if (!strncmp(displayTitleTerm, p->term, strlen(p->term))) {
                title_str = p->title_str;
                break;
            }
        }
    }
}

void close_tty(void)
{
    if (g_tty > 2)
        close(g_tty);
}

char* ttyname_tty(void)
{
    return ttyname(g_tty);
}

void term_raw(void)
#ifndef HAVE_SGTTY_H
#ifdef IEXTEN
#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
#else /* not IEXTEN */
#define TTY_MODE ISIG | ICANON | ECHO
#endif /* not IEXTEN */
{
    ttymode_reset(TTY_MODE, IXON | IXOFF);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 1);
#else /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else /* HAVE_SGTTY_H */
{
    ttymode_set(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void ttymode_set(int mode, int imode)
{
    TerminalMode ioval;
    _TerminalGet(g_tty, &ioval);
    MODEFLAG(ioval) |= mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) |= imode;
#endif /* not HAVE_SGTTY_H */

    while (_TerminalSet(g_tty, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while set %x: errno=%d\n", mode, errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        exit(9);
    }
}

void ttymode_reset(int mode, int imode)
{
    TerminalMode ioval;
    _TerminalGet(g_tty, &ioval);
    MODEFLAG(ioval) &= ~mode;
#ifndef HAVE_SGTTY_H
    IMODEFLAG(ioval) &= ~imode;
#endif /* not HAVE_SGTTY_H */

    while (_TerminalSet(g_tty, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while reset %x: errno=%d\n", mode, errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        exit(9);
    }
}

void set_cc(int spec, int val)
{
    TerminalMode ioval;
    _TerminalGet(g_tty, &ioval);
    ioval.c_cc[spec] = val;
    while (_TerminalSet(g_tty, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred: errno=%d\n", errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        exit(9);
    }
}

void crmode(void)
#ifndef HAVE_SGTTY_H
{
    ttymode_reset(ICANON, IXON);
    ttymode_set(ISIG, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 1);
#else /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else /* HAVE_SGTTY_H */
{
    ttymode_set(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void nocrmode(void)
#ifndef HAVE_SGTTY_H
{
    ttymode_set(ICANON, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 4);
#else /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else /* HAVE_SGTTY_H */
{
    ttymode_reset(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void term_echo(void)
{
    ttymode_set(ECHO, 0);
}

void term_noecho(void)
{
    ttymode_reset(ECHO, 0);
}

void term_cooked(void)
#ifndef HAVE_SGTTY_H
{
    ttymode_set(TTY_MODE, 0);
#ifdef HAVE_TERMIOS_H
    set_cc(VMIN, 4);
#else /* not HAVE_TERMIOS_H */
    set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else /* HAVE_SGTTY_H */
{
    ttymode_reset(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void term_cbreak(void)
{
    term_cooked();
    term_noecho();
}

static void
skip_escseq(GetChFunc getch)
{
    int c = getch();
    if (c == '[' || c == 'O') {
        c = getch();
        while (IS_DIGIT(c))
            c = getch();
    }
}

int sleep_till_anykey(int timeout_ms, int purge)
{
    TerminalMode ioval;
    _TerminalGet(g_tty, &ioval);
    term_raw();

    GetChFunc getch = event_begin_input(timeout_ms);
    int c = getch();
    if (c == ESC_CODE)
        skip_escseq(getch);
    event_end_input(getch);

    int er = _TerminalSet(g_tty, &ioval);
    if (er == -1) {
        printf("Error occurred: errno=%d\n", errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        exit(9);
    }
    return c;
}

int get_pixel_per_cell(int* ppc, int* ppl)
{
    fd_set rfd;
    struct timeval tval;
    char buf[100];
    char* p;
    ssize_t len;
    ssize_t left;
    int wp, hp, wc, hc;
    int i;

#ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(g_tty, TIOCGWINSZ, &ws) == 0 && ws.ws_ypixel > 0 && ws.ws_row > 0 && ws.ws_xpixel > 0 && ws.ws_col > 0) {
        *ppc = ws.ws_xpixel / ws.ws_col;
        *ppl = ws.ws_ypixel / ws.ws_row;
        return 1;
    }
#endif

    fputs("\x1b[14t\x1b[18t", g_ttyf);
    flush_tty();

    p = buf;
    left = sizeof(buf) - 1;
    for (i = 0; i < 10; i++) {
        tval.tv_usec = 200000; /* 0.2 sec * 10 */
        tval.tv_sec = 0;
        FD_ZERO(&rfd);
        FD_SET(g_tty, &rfd);
        if (select(g_tty + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(g_tty, &rfd))
            continue;

        if ((len = read(g_tty, p, left)) <= 0)
            continue;
        p[len] = '\0';

        if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4) {
            if (wp > 0 && wc > 0 && hp > 0 && hc > 0) {
                *ppc = wp / wc;
                *ppl = hp / hc;
                return 1;
            } else {
                return 0;
            }
        }
        p += len;
        left -= len;
    }

    return 0;
}

void term_title(const char* s)
{
    if (title_str != NULL) {
        fprintf(g_ttyf, title_str, s);
    }
}
