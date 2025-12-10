/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "terms.h"
#include "config.h"
#include "funcheader.h"
#include "screen.h"
#include "term_entry.h"
#include "fm.h"

#include <wc/wtf.h>
#include <gcstr.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termcap.h>
#include <termios.h>
#include <unistd.h>

static int is_xterm = 0;

static char* title_str = NULL;

static struct termios d_ioval;
static int tty = -1;
static FILE* ttyf = NULL;

#define MAX_LINE 200
#define MAX_COLUMN 400

struct TermSize g_size = {
    .lines = 0,
    .cols = 0,
};
struct TermSize tty_current_size()
{
    return g_size;
}

void tty_immediate_setattr(struct termios* p)
{
    while (true) {
        if (tcsetattr(tty, TCSANOW, p) == 0) {
            // success
            break;
        }

        if (errno == EINTR || errno == EAGAIN) {
            // retry
            continue;
        }

        // exit
        fprintf(stderr, "[error] tcsetattr: %d => %s\n", errno, strerror(errno));
        w3m_exit(1);
        break;
    }
}

int tty_putc(int c)
{
    putc(c, ttyf);
    return 0;
}

void tty_wc_putc(char* c)
{
    wc_putc(c, ttyf);
}

void tty_wc_putc_end()
{
    wc_putc_end(ttyf);
}

void tty_write(const char* s)
{
    tputs(s, 1, tty_putc);
}

void tty_move(int line, int column)
{
    tty_write(tgoto(T_.cm, column, line));
}

int tty_get_pixel_per_cell(int* ppc, int* ppl)
{
    fd_set rfd;
    struct timeval tval;
    char buf[100];
    char* p;
    ssize_t len;
    ssize_t left;
    int wp, hp, wc, hc;
    int i;

    fputs("\x1b[14t\x1b[18t", ttyf);
    tty_flush();

    p = buf;
    left = sizeof(buf) - 1;
    for (i = 0; i < 10; i++) {
        tval.tv_usec = 200000; /* 0.2 sec * 10 */
        tval.tv_sec = 0;
        FD_ZERO(&rfd);
        FD_SET(tty, &rfd);
        if (select(tty + 1, &rfd, NULL, NULL, &tval) <= 0 || !FD_ISSET(tty, &rfd))
            continue;

        if ((len = read(tty, p, left)) <= 0)
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

#define W3M_TERM_INFO(name, title, mouse) name, title, mouse
#define NEED_XTERM_ON (1)
#define NEED_XTERM_OFF (1 << 1)

static char XTERM_TITLE[] = "\033]0;w3m: %s\007";
static char SCREEN_TITLE[] = "\033k%s\033\134";

static struct w3m_term_info {
    char* term;
    char* title_str;
    int mouse_flag;
} w3m_term_info_list[] = {
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
    char* ttyn;

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
    if (displayTitleTerm != NULL) {
        struct w3m_term_info* p;
        for (p = w3m_term_info_list; p->term != NULL; p++) {
            if (!strncmp(displayTitleTerm, p->term, strlen(p->term))) {
                title_str = p->title_str;
                break;
            }
        }
    }
    {
        char* term = getenv("TERM");
        if (term != NULL) {
            struct w3m_term_info* p;
            for (p = w3m_term_info_list; p->term != NULL; p++) {
                if (!strncmp(term, p->term, strlen(p->term))) {
                    is_xterm = p->mouse_flag;
                    break;
                }
            }
        }
    }
    return 0;
}

void ttymode_add(int local_mode, int input_mode)
{
    struct termios ioval;
    tcgetattr(tty, &ioval);
    ioval.c_lflag |= local_mode;
    ioval.c_iflag |= input_mode;
    tty_immediate_setattr(&ioval);
}

void ttymode_remove(int local_mode, int input_mode)
{
    struct termios ioval;
    tcgetattr(tty, &ioval);
    ioval.c_lflag &= ~local_mode;
    ioval.c_iflag &= ~input_mode;
    tty_immediate_setattr(&ioval);
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
        w3m_exit(0);
    }
}

void close_tty(void)
{
    if (tty > 2)
        close(tty);
}

char* tty_name(void)
{
    return ttyname(tty);
}

void tty_reset(void)
{
    tty_write(T_.op); /* turn off */
    tty_write(T_.me);
    if (!Do_not_use_ti_te) {
        if (T_.te && *T_.te)
            tty_write(T_.te);
        else
            tty_write(T_.cl);
    }
    tty_write(T_.se); /* reset terminal */
    tty_flush();
    tcsetattr(tty, TCSANOW, &d_ioval);
    if (tty != 2)
        close_tty();
}

void tty_update_size()
{
    struct TermSize size = {
        .lines = -1,
        .cols = -1,
    };

    // TODO: termios

    const char* p;
    int i;
    if ((p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        size.lines = i;
    if ((p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        size.cols = i;
    if (size.lines <= 0)
        size.lines = tgetnum("li"); /* number of line */
    if (size.cols <= 0)
        size.cols = tgetnum("co"); /* number of column */

    g_size = size;
    if (g_size.cols > MAX_COLUMN)
        g_size.cols = MAX_COLUMN;
    if (g_size.lines > MAX_LINE)
        g_size.lines = MAX_LINE;
}

/*
 * struct ScreenLine initialize
 */
bool initscr(void)
{
    if (set_tty() < 0) {
        return false;
    }

    // termcap
    getTCstr();
    if (T_.ti && !Do_not_use_ti_te) {
        tty_write(T_.ti);
    }

    // screen
    tty_update_size();
    scr_setup(g_size.lines, g_size.cols);

    return true;
}

// void tty_crmode(void)
// {
//     ttymode_reset(ICANON, IXON);
//     ttymode_set(ISIG, 0);
//     set_cc(VMIN, 1);
// }

void tty_nocrmode(void)
{
    ttymode_add(ICANON, 0);
    set_cc(VMIN, 4);
}

void tty_echo(void)
{
    ttymode_add(ECHO, 0);
}

void tty_noecho(void)
{
    ttymode_remove(ECHO, 0);
}

void tty_raw(void)
{
    ttymode_remove(ISIG | ICANON | ECHO | IEXTEN, IXON | IXOFF | IGNCR | INLCR | ICRNL);
    set_cc(VMIN, 1);
}

void tty_cooked(void)
{
    ttymode_add(ISIG | ICANON | ECHO | IEXTEN, 0);
    set_cc(VMIN, 4);
}

void tty_cbreak(void)
{
    tty_cooked();
    tty_noecho();
}

void tty_set_title(const char* s)
{
    if (title_str != NULL) {
        fprintf(ttyf, title_str, s);
    }
}

char getch(void)
{
    char c;
    while (read(tty, &c, 1) < (int)1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        quitfm();
        break; /* unreachable */
    }
    return c;
}

void tty_bell()
{
    tty_putc(7);
}

static void
skip_escseq(void)
{
    int c = getch();
    if (c == '[' || c == 'O') {
        c = getch();
        if (is_xterm && c == 'M') {
            getch();
            getch();
            getch();
        } else if (is_xterm && c == '<') {
            c = getch();
            while (IS_DIGIT(c) || c == ';')
                c = getch();
        } else
            while (IS_DIGIT(c))
                c = getch();
    }
}

int tty_sleep_till_anykey(int sec, int purge)
{
    fd_set rfd;
    struct timeval tim;
    int er, c, ret;
    struct termios ioval;

    tcgetattr(tty, &ioval);
    tty_raw();

    tim.tv_sec = sec;
    tim.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(tty, &rfd);

    ret = select(tty + 1, &rfd, 0, 0, &tim);
    if (ret > 0 && purge) {
        c = getch();
        if (c == ESC_CODE)
            skip_escseq();
    }
    er = tcsetattr(tty, TCSANOW, &ioval);
    if (er == -1) {
        printf("Error occurred: errno=%d\n", errno);
        w3m_exit(0);
    }
    return ret;
}

void tty_flush(void)
{
    if (ttyf)
        fflush(ttyf);
}
