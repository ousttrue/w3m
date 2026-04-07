#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include "term_tty.h"
#include "global.h"
#include "constants.h"
#include "ctrlcode.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static struct termios d_ioval;
static int tty = -1;
static FILE* ttyf = NULL;

#define W3M_TERM_INFO(name, title, mouse) name, title

static char XTERM_TITLE[] = "\033]0;w3m: %s\007";
static char SCREEN_TITLE[] = "\033k%s\033\134";

struct w3m_term_info {
    const char* term;
    const char* title_str;
};
static struct w3m_term_info w3m_term_info_list[] = {
    { W3M_TERM_INFO("xterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("kterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("rxvt", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("Eterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("mlterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF)) },
    { W3M_TERM_INFO("screen", SCREEN_TITLE, 0) },
    { W3M_TERM_INFO(NULL, NULL, 0) }
};

int set_tty(void)
{
    const char* ttyn;
    if (isatty(0)) /* stdin */
        ttyn = ttyname(0);
    else
        ttyn = DEV_TTY_PATH;
    tty = open(ttyn, O_RDWR);
    if (tty < 0) {
        /* use stderr instead of stdin... is it OK???? */
        tty = 2;
    }
    ttyf = fdopen(tty, "w");
    tcgetattr(tty, &d_ioval);
    return 0;
}

char* ttyname_tty(void)
{
    return ttyname(tty);
}

void setlinescols(void)
{
    char* p;
    int i;

    struct winsize wins;

    i = ioctl(tty, TIOCGWINSZ, &wins);
    if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0) {
        LINES = wins.ws_row;
        COLS = wins.ws_col;
    }

    // if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
    //     LINES = i;
    // if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
    //     COLS = i;
    // if (LINES <= 0)
    //     LINES = tgetnum("li"); /* number of line */
    // if (COLS <= 0)
    //     COLS = tgetnum("co"); /* number of column */
    if (COLS > MAX_COLUMN)
        COLS = MAX_COLUMN;
    if (LINES > MAX_LINE)
        LINES = MAX_LINE;
}

void clear_tty(void)
{
    flush_tty();
    tcsetattr(tty, TCSANOW, &d_ioval);
    if (tty != 2)
        close_tty();
}

void close_tty(void)
{
    if (tty > 2)
        close(tty);
}

void set_cc(int spec, int val)
{
    struct termios ioval;

    tcgetattr(tty, &ioval);
    ioval.c_cc[spec] = val;
    while (tcsetattr(tty, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred: errno=%d\n", errno);
        // reset_error_exit(SIGNAL_ARGLIST);
        exit(1);
    }
}

void ttymode_add(int mode, int imode)
{
    struct termios ioval;

    tcgetattr(tty, &ioval);
    ioval.c_lflag |= mode;
    ioval.c_iflag |= imode;

    while (tcsetattr(tty, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while set %x: errno=%d\n", mode, errno);
        exit(1);
    }
}

void crmode(void)
{
    ttymode_remove(ICANON, IXON);
    ttymode_add(ISIG, 0);
    set_cc(VMIN, 1);
}

void nocrmode(void)
{
    ttymode_add(ICANON, 0);
    set_cc(VMIN, 4);
}

void term_echo(void)
{
    ttymode_add(ECHO, 0);
}

void term_noecho(void)
{
    ttymode_remove(ECHO, 0);
}

#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN

void term_raw(void)
{
    ttymode_remove(TTY_MODE, IXON | IXOFF | INLCR | IGNCR | ICRNL);
    set_cc(VMIN, 1);
}

void term_cooked(void)
{
    ttymode_add(TTY_MODE, 0);
    set_cc(VMIN, 4);
}

void term_cbreak(void)
{
    term_cooked();
    term_noecho();
}

void ttymode_remove(int mode, int imode)
{
    struct termios ioval;

    tcgetattr(tty, &ioval);
    ioval.c_lflag &= ~mode;
    ioval.c_iflag &= ~imode;

    while (tcsetattr(tty, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while reset %x: errno=%d\n", mode, errno);
        exit(1);
    }
}

void flush_tty(void)
{
    if (ttyf)
        fflush(ttyf);
}

bool get_pixel_per_cell(int* ppc, int* ppl)
{
    fputs("\x1b[14t\x1b[18t", ttyf);
    flush_tty();

    int wp, hp, wc, hc;

    char buf[100];
    char* p = buf;
    ssize_t left = sizeof(buf) - 1;
    for (int i = 0; i < 10; i++) {
        struct timeval tval;
        tval.tv_usec = 200000; /* 0.2 sec * 10 */
        tval.tv_sec = 0;
        fd_set rfd;
        FD_ZERO(&rfd);
        FD_SET(tty, &rfd);
        if (select(tty + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(tty, &rfd))
            continue;

        ssize_t len;
        if ((len = read(tty, p, left)) <= 0)
            continue;
        p[len] = '\0';

        if (sscanf(buf, "\x1b[4;%d;%dt\x1b[8;%d;%dt", &hp, &wp, &hc, &wc) == 4) {
            if (wp > 0 && wc > 0 && hp > 0 && hc > 0) {
                *ppc = wp / wc;
                *ppl = hp / hc;
                return true;
            } else {
                return false;
            }
        }
        p += len;
        left -= len;
    }

    return false;
}

void writer(const uint8_t* str, size_t len)
{
    fwrite(str, 1, len, ttyf);
}

int write1(int c)
{
    putc(c, ttyf);
    return 0;
}

void bell(void)
{
    write1(7);
}

char getch(void)
{
    char c;
    while (read(tty, &c, 1) < (int)1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        // quitfm((struct CmdArgs) { 0 });
        exit(1);
        break; /* unreachable */
    }
    return c;
}

static void skip_escseq(void)
{
    int c = getch();
    if (c == '[' || c == 'O') {
        c = getch();
        while (IS_DIGIT(c))
            c = getch();
    }
}

int sleep_till_anykey(int sec, int purge)
{
    fd_set rfd;
    struct timeval tim;
    int er, c;
    struct termios ioval;

    tcgetattr(tty, &ioval);
    term_raw();

    tim.tv_sec = sec;
    tim.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(tty, &rfd);

    int ret = select(tty + 1, &rfd, 0, 0, &tim);
    if (ret > 0 && purge) {
        c = getch();
        if (c == ESC_CODE)
            skip_escseq();
    }
    er = tcsetattr(tty, TCSANOW, &ioval);
    if (er == -1) {
        printf("Error occurred: errno=%d\n", errno);
        exit(1);
    }
    return ret;
}
