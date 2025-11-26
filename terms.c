/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "terms.h"
#include "w3m_runtime.h"
#include "config.h"
#include "ctrlcode.h"
#include "funcheader.h"
#include "screen.h"
#include "term_entry.h"
#include "fm.h"

#include <wtf.h>
#include <gcstr/gcstr.h>
#include <stdio.h>
#include <signal.h>
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

static struct TermEntry T_;

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

typedef struct termios TerminalMode;
#define TerminalSet(fd, x) tcsetattr(fd, TCSANOW, x)
#define TerminalGet(fd, x) tcgetattr(fd, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)

static TerminalMode d_ioval;
static int tty = -1;
static FILE* ttyf = NULL;

#define MAX_LINE 200
#define MAX_COLUMN 400
int LINES, COLS;

static void reset_exit_with_value(int _, int rval)
{
    reset_tty();
    w3m_exit(rval);
}

static void reset_error_exit(int _)
{
    reset_exit_with_value(0, 1);
}

static void reset_exit(int _)
{
    reset_exit_with_value(0, 0);
}

void setlinescols(int lines, int cols)
{
    LINES = lines;
    COLS = cols;
    if (COLS > MAX_COLUMN)
        COLS = MAX_COLUMN;
    if (LINES > MAX_LINE)
        LINES = MAX_LINE;
}

static int graph_enabled = 0;

static int tty_putc(int c)
{
    putc(c, ttyf);
    return 0;
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
    TerminalGet(tty, &d_ioval);
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

void ttymode_set(int mode, int imode)
{
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    MODEFLAG(ioval) |= mode;
    IMODEFLAG(ioval) |= imode;

    while (TerminalSet(tty, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while set %x: errno=%d\n", mode, errno);
        reset_error_exit(0);
    }
}

void ttymode_reset(int mode, int imode)
{
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    MODEFLAG(ioval) &= ~mode;
    IMODEFLAG(ioval) &= ~imode;

    while (TerminalSet(tty, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while reset %x: errno=%d\n", mode, errno);
        reset_error_exit(0);
    }
}

void set_cc(int spec, int val)
{
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    ioval.c_cc[spec] = val;
    while (TerminalSet(tty, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred: errno=%d\n", errno);
        reset_error_exit(0);
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

void reset_tty(void)
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
    TerminalSet(tty, &d_ioval);
    if (tty != 2)
        close_tty();
}

static void error_dump(int _)
{
    mySignal(SIGIOT, SIG_DFL);
    reset_tty();
    abort();
}

void set_int(void)
{
    mySignal(SIGHUP, reset_exit);
    mySignal(SIGINT, reset_exit);
    mySignal(SIGQUIT, reset_exit);
    mySignal(SIGTERM, reset_exit);
    mySignal(SIGILL, error_dump);
    mySignal(SIGIOT, error_dump);
    mySignal(SIGFPE, error_dump);
#ifdef SIGBUS
    mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
    /* mySignal(SIGSEGV, error_dump); */
}

#define graphchar(c) (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? T_.gcmap[(c) - ' '] : (c))

struct TermSize get_term_size()
{
    struct TermSize size = {
        .lines = -1,
        .cols = -1,
    };
    char* p;
    int i;
    if ((p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        size.lines = i;
    if ((p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        size.cols = i;
    if (size.lines <= 0)
        size.lines = tgetnum("li"); /* number of line */
    if (size.cols <= 0)
        size.cols = tgetnum("co"); /* number of column */
    return size;
}

/*
 * struct ScreenLine initialize
 */
int initscr(void)
{
    if (set_tty() < 0)
        return -1;
    set_int();
    getTCstr(&T_);

    if (T_.ti && !Do_not_use_ti_te)
        tty_write(T_.ti);

    struct TermSize size = get_term_size();
    setupscreen(size.lines, size.cols);
    return 0;
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return T_.as[0] != 0 && T_.ae[0] != 0 && T_.ac[0] != 0;
}

static char*
color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    return seqbuf;
}

static char*
bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

#define SPACE " "
#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS)
void refresh(void)
{
    struct Screen sc = scr_get();

    int line, col, pcol;
    int pline = sc.CurLine;
    int moved = RF_NEED_TO_MOVE;
    uint16_t *pr, mode = 0;
    uint16_t color = COL_FTERM;
    uint16_t bcolor = COL_BTERM;
    short* dirty;

    wc_putc_init(InnerCharset, DisplayCharset);
    for (line = 0; line <= LASTLINE; line++) {
        dirty = &sc.ScreenImage[line]->isdirty;
        if (*dirty & L_DIRTY) {
            *dirty &= ~L_DIRTY;
            char** pc;
            pc = sc.ScreenImage[line]->lineimage;
            pr = sc.ScreenImage[line]->lineprop;
            for (col = 0; col < COLS && !(pr[col] & S_EOL); col++) {
                if (*dirty & L_NEED_CE && col >= sc.ScreenImage[line]->eol) {
                    if (scr_is_need_redraw(pc[col], pr[col], SPACE, 0))
                        break;
                } else {
                    if (pr[col] & S_DIRTY)
                        break;
                }
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                pcol = sc.ScreenImage[line]->eol;
                if (pcol >= COLS) {
                    *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
                    pcol = col;
                }
            } else {
                pcol = col;
            }
            if (line < LINES - 2 && pline == line - 1 && pcol == 0) {
                switch (moved) {
                case RF_NEED_TO_MOVE:
                    tty_move(line, 0);
                    moved = RF_CR_OK;
                    break;
                case RF_CR_OK:
                    tty_putc('\n');
                    tty_putc('\r');
                    break;
                case RF_NONEED_TO_MOVE:
                    moved = RF_CR_OK;
                    break;
                }
            } else {
                tty_move(line, pcol);
                moved = RF_CR_OK;
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                tty_write(T_.ce);
                if (col != pcol)
                    tty_move(line, col);
            }
            pline = line;
            pcol = col;
            for (; col < COLS; col++) {
                if (pr[col] & S_EOL)
                    break;

                /*
                 * some terminal emulators do linefeed when a
                 * character is put on COLS-th column. this behavior
                 * is different from one of vt100, but such terminal
                 * emulators are used as vt100-compatible
                 * emulators. This behaviour causes scroll when a
                 * character is drawn on (COLS-1,LINES-1) point.  To
                 * avoid the scroll, I prohibit to draw character on
                 * (COLS-1,LINES-1).
                 */
#if !defined(USE_BG_COLOR) || defined(__CYGWIN__)
                if (line == LINES - 1 && col == COLS - 1)
                    break;
#endif /* !defined(USE_BG_COLOR) || defined(__CYGWIN__) */
                if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) || (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) || (!(pr[col] & S_BOLD) && (mode & S_BOLD)) || (!(pr[col] & S_COLORED) && (mode & S_COLORED))
                    || (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED))
                    || (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS))) {
                    if ((mode & S_COLORED)
                        || (mode & S_BCOLORED))
                        tty_write(T_.op);
                    if (mode & S_GRAPHICS)
                        tty_write(T_.ae);
                    tty_write(T_.me);
                    mode &= ~M_MEND;
                }
                if ((*dirty & L_NEED_CE && col >= sc.ScreenImage[line]->eol) ? scr_is_need_redraw(pc[col], pr[col], SPACE,
                                                                                   0)
                                                                             : (pr[col] & S_DIRTY)) {
                    if (pcol == col - 1)
                        tty_write(T_.nd);
                    else if (pcol != col)
                        tty_move(line, col);

                    if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
                        tty_write(T_.so);
                        mode |= S_STANDOUT;
                    }
                    if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
                        tty_write(T_.us);
                        mode |= S_UNDERLINE;
                    }
                    if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
                        tty_write(T_.md);
                        mode |= S_BOLD;
                    }
                    if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
                        color = (pr[col] & COL_FCOLOR);
                        mode = ((mode & ~COL_FCOLOR) | color);
                        tty_write(color_seq(color));
                    }
                    if ((pr[col] & S_BCOLORED)
                        && (pr[col] ^ mode) & COL_BCOLOR) {
                        bcolor = (pr[col] & COL_BCOLOR);
                        mode = ((mode & ~COL_BCOLOR) | bcolor);
                        tty_write(bcolor_seq(bcolor));
                    }
                    if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
                        wc_putc_end(ttyf);
                        if (!graph_enabled) {
                            graph_enabled = 1;
                            tty_write(T_.eA);
                        }
                        tty_write(T_.as);
                        mode |= S_GRAPHICS;
                    }
                    if (pr[col] & S_GRAPHICS)
                        tty_putc(graphchar(*pc[col]));
                    else if (CHMODE(pr[col]) != C_WCHAR2)
                        wc_putc(pc[col], ttyf);
                    pcol = col + 1;
                }
            }
            if (col == COLS)
                moved = RF_NEED_TO_MOVE;
            for (; col < COLS && !(pr[col] & S_EOL); col++)
                pr[col] |= S_EOL;
        }
        *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
        if (mode & M_MEND) {
            if (mode & (S_COLORED | S_BCOLORED))
                tty_write(T_.op);
            if (mode & S_GRAPHICS) {
                tty_write(T_.ae);
                wc_putc_clear_status();
            }
            tty_write(T_.me);
            mode &= ~M_MEND;
        }
    }
    wc_putc_end(ttyf);
    tty_move(sc.CurLine, sc.CurColumn);
    tty_flush();
}


void crmode(void)
{
    ttymode_reset(ICANON, IXON);
    ttymode_set(ISIG, 0);
    set_cc(VMIN, 1);
}

void nocrmode(void)
{
    ttymode_set(ICANON, 0);
    set_cc(VMIN, 4);
}

void term_echo(void)
{
    ttymode_set(ECHO, 0);
}

void term_noecho(void)
{
    ttymode_reset(ECHO, 0);
}

void term_raw(void)
#ifdef IEXTEN
#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
#else /* not IEXTEN */
#define TTY_MODE ISIG | ICANON | ECHO
#endif /* not IEXTEN */
{
    ttymode_reset(TTY_MODE, IXON | IXOFF | INLCR | IGNCR | ICRNL);
    set_cc(VMIN, 1);
}

void term_cooked(void)
{
    ttymode_set(TTY_MODE, 0);
    set_cc(VMIN, 4);
}

void term_cbreak(void)
{
    term_cooked();
    term_noecho();
}

void term_title(char* s)
{
    if (!fmInitialized)
        return;
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
    int c;

    c = getch();
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

int sleep_till_anykey(int sec, int purge)
{
    fd_set rfd;
    struct timeval tim;
    int er, c, ret;
    TerminalMode ioval;

    TerminalGet(tty, &ioval);
    term_raw();

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
    er = TerminalSet(tty, &ioval);
    if (er == -1) {
        printf("Error occurred: errno=%d\n", errno);
        reset_error_exit(0);
    }
    return ret;
}

void tty_flush(void)
{
    if (ttyf)
        fflush(ttyf);
}
