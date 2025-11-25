/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "w3m_runtime.h"
#include "config.h"
// #include "mimetype.h"
// #include "etc.h"
// #include "buffer.h"
#include "ctrlcode.h"
#include "funcheader.h"
#include "screen.h"
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

static int is_xterm = 0;

void mouse_init(void), mouse_end(void);

static char* title_str = NULL;

static int tty;

#include "terms.h"
#include "fm.h"

char* getenv(const char*);
void reset_exit(int);
void reset_error_exit(int);
void error_dump(int);
void flush_tty(void);

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#include <termios.h>
#include <unistd.h>
typedef struct termios TerminalMode;
#define TerminalSet(fd, x) tcsetattr(fd, TCSANOW, x)
#define TerminalGet(fd, x) tcgetattr(fd, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)

static TerminalMode d_ioval;
static int tty = -1;
static FILE* ttyf = NULL;

static char bp[1024], funcstr[256];

char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
    *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
    *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

#define MAX_LINE 200
#define MAX_COLUMN 400
int LINES, COLS;
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

static char gcmap[96];

void clear(void), wrap(void), touch_line(void);
void clrtoeol(void); /* conflicts with curs_clear(3)? */

static int write1(int c)
{
    putc(c, ttyf);
    return 0;
}

void writestr(const char* s)
{
    tputs(s, 1, write1);
}

void MOVE(int line, int column)
{
    writestr(tgoto(T_cm, column, line));
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

    fputs("\x1b[14t\x1b[18t", ttyf);
    flush_tty();

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

char* ttyname_tty(void)
{
    return ttyname(tty);
}

void reset_tty(void)
{
    writestr(T_op); /* turn off */
    writestr(T_me);
    if (!Do_not_use_ti_te) {
        if (T_te && *T_te)
            writestr(T_te);
        else
            writestr(T_cl);
    }
    writestr(T_se); /* reset terminal */
    flush_tty();
    TerminalSet(tty, &d_ioval);
    if (tty != 2)
        close_tty();
}

static void
reset_exit_with_value(int _, int rval)
{
    reset_tty();
    w3m_exit(rval);
}

void reset_error_exit(int _)
{
    reset_exit_with_value(0, 1);
}

void reset_exit(int _)
{
    reset_exit_with_value(0, 0);
}

void error_dump(int _)
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

static void
setgraphchar(void)
{
    int c, i, n;

    for (c = 0; c < 96; c++)
        gcmap[c] = (char)(c + ' ');

    if (!T_ac)
        return;

    n = strlen(T_ac);
    for (i = 0; i < n - 1; i += 2) {
        c = (unsigned)T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            gcmap[c] = T_ac[i + 1];
    }
}

#define graphchar(c) (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c))
#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

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

void getTCstr(void)
{
    char* ent;
    char* suc;
    char* pt = funcstr;
    int r;

    ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        reset_error_exit(0);
    }

    r = tgetent(bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        reset_error_exit(0);
    }

    GETSTR(T_ce, "ce"); /* clear to the end of line */
    GETSTR(T_cd, "cd"); /* clear to the end of display */
    GETSTR(T_kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(T_kr, "kr");
    if (tgetflag("bs"))
        T_kl = "\b"; /* cursor left */
    else {
        GETSTR(T_kl, "le");
        if (suc == NULL)
            GETSTR(T_kl, "kb");
        if (suc == NULL)
            GETSTR(T_kl, "kl");
    }
    GETSTR(T_cr, "cr"); /* carriage return */
    GETSTR(T_ta, "ta"); /* tab */
    GETSTR(T_sc, "sc"); /* save cursor */
    GETSTR(T_rc, "rc"); /* restore cursor */
    GETSTR(T_so, "so"); /* standout mode */
    GETSTR(T_se, "se"); /* standout mode end */
    GETSTR(T_us, "us"); /* underline mode */
    GETSTR(T_ue, "ue"); /* underline mode end */
    GETSTR(T_md, "md"); /* bold mode */
    GETSTR(T_me, "me"); /* bold mode end */
    GETSTR(T_cl, "cl"); /* clear screen */
    GETSTR(T_cm, "cm"); /* cursor move */
    GETSTR(T_al, "al"); /* append line */
    GETSTR(T_sr, "sr"); /* scroll reverse */
    GETSTR(T_ti, "ti"); /* terminal init */
    GETSTR(T_te, "te"); /* terminal end */
    GETSTR(T_nd, "nd"); /* move right one space */
    GETSTR(T_eA, "eA"); /* enable alternative charset */
    GETSTR(T_as, "as"); /* alternative (graphic) charset start */
    GETSTR(T_ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(T_ac, "ac"); /* graphics charset pairs */
    GETSTR(T_op, "op"); /* set default color pair to its original value */

    // LINES = COLS = 0;
    struct TermSize size = get_term_size();
    setlinescols(size.lines, size.cols);
    setgraphchar();
}

/*
 * struct ScreenLine initialize
 */
int initscr(void)
{
    if (set_tty() < 0)
        return -1;
    set_int();
    getTCstr();
    if (T_ti && !Do_not_use_ti_te)
        writestr(T_ti);

    struct TermSize size = get_term_size();
    setupscreen(size.lines, size.cols);
    return 0;
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return T_as[0] != 0 && T_ae[0] != 0 && T_ac[0] != 0;
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
    struct Screen sc = getScreen();

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
                    if (is_need_redraw(pc[col], pr[col], SPACE, 0))
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
                    MOVE(line, 0);
                    moved = RF_CR_OK;
                    break;
                case RF_CR_OK:
                    write1('\n');
                    write1('\r');
                    break;
                case RF_NONEED_TO_MOVE:
                    moved = RF_CR_OK;
                    break;
                }
            } else {
                MOVE(line, pcol);
                moved = RF_CR_OK;
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                writestr(T_ce);
                if (col != pcol)
                    MOVE(line, col);
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
                        writestr(T_op);
                    if (mode & S_GRAPHICS)
                        writestr(T_ae);
                    writestr(T_me);
                    mode &= ~M_MEND;
                }
                if ((*dirty & L_NEED_CE && col >= sc.ScreenImage[line]->eol) ? is_need_redraw(pc[col], pr[col], SPACE,
                                                                                   0)
                                                                             : (pr[col] & S_DIRTY)) {
                    if (pcol == col - 1)
                        writestr(T_nd);
                    else if (pcol != col)
                        MOVE(line, col);

                    if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
                        writestr(T_so);
                        mode |= S_STANDOUT;
                    }
                    if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
                        writestr(T_us);
                        mode |= S_UNDERLINE;
                    }
                    if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
                        writestr(T_md);
                        mode |= S_BOLD;
                    }
                    if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
                        color = (pr[col] & COL_FCOLOR);
                        mode = ((mode & ~COL_FCOLOR) | color);
                        writestr(color_seq(color));
                    }
                    if ((pr[col] & S_BCOLORED)
                        && (pr[col] ^ mode) & COL_BCOLOR) {
                        bcolor = (pr[col] & COL_BCOLOR);
                        mode = ((mode & ~COL_BCOLOR) | bcolor);
                        writestr(bcolor_seq(bcolor));
                    }
                    if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
                        wc_putc_end(ttyf);
                        if (!graph_enabled) {
                            graph_enabled = 1;
                            writestr(T_eA);
                        }
                        writestr(T_as);
                        mode |= S_GRAPHICS;
                    }
                    if (pr[col] & S_GRAPHICS)
                        write1(graphchar(*pc[col]));
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
                writestr(T_op);
            if (mode & S_GRAPHICS) {
                writestr(T_ae);
                wc_putc_clear_status();
            }
            writestr(T_me);
            mode &= ~M_MEND;
        }
    }
    wc_putc_end(ttyf);
    MOVE(sc.CurLine, sc.CurColumn);
    flush_tty();
}

#ifdef USE_RAW_SCROLL
static void
scroll_raw(void)
{ /* raw scroll */
    MOVE(LINES - 1, 0);
    write1('\n');
}

void scroll(int n)
{ /* scroll up */
    int cli = CurLine, cco = CurColumn;
    struct ScreenLine* t;
    int i, j, k;

    i = LINES;
    j = n;
    do {
        k = j;
        j = i % k;
        i = k;
    } while (j);
    do {
        k--;
        i = k;
        j = (i + n) % LINES;
        t = ScreenImage[k];
        while (j != k) {
            ScreenImage[i] = ScreenImage[j];
            i = j;
            j = (i + n) % LINES;
        }
        ScreenImage[i] = t;
    } while (k);

    for (i = 0; i < n; i++) {
        t = ScreenImage[LINES - 1 - i];
        t->isdirty = 0;
        for (j = 0; j < COLS; j++)
            t->lineprop[j] = S_EOL;
        scroll_raw();
    }
    move(cli, cco);
}

void rscroll(int n)
{ /* scroll down */
    int cli = CurLine, cco = CurColumn;
    struct ScreenLine* t;
    int i, j, k;

    i = LINES;
    j = n;
    do {
        k = j;
        j = i % k;
        i = k;
    } while (j);
    do {
        k--;
        i = k;
        j = (LINES + i - n) % LINES;
        t = ScreenImage[k];
        while (j != k) {
            ScreenImage[i] = ScreenImage[j];
            i = j;
            j = (LINES + i - n) % LINES;
        }
        ScreenImage[i] = t;
    } while (k);
    if (T_sr && *T_sr) {
        MOVE(0, 0);
        for (i = 0; i < n; i++) {
            t = ScreenImage[i];
            t->isdirty = 0;
            for (j = 0; j < COLS; j++)
                t->lineprop[j] = S_EOL;
            writestr(T_sr);
        }
        move(cli, cco);
    } else {
        for (i = 0; i < LINES; i++) {
            t = ScreenImage[i];
            t->isdirty |= L_DIRTY | L_NEED_CE;
            for (j = 0; j < COLS; j++) {
                t->lineprop[j] |= S_DIRTY;
            }
        }
    }
}
#endif

void addstr(char* s)
{
    int len;

    while (*s != '\0') {
        len = wtf_len((wc_uchar*)s);
        addmch(s, len);
        s += len;
    }
}

void addnstr(char* s, int n)
{
    int i;
    int len, width;

    for (i = 0; *s != '\0';) {
        width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        len = wtf_len((wc_uchar*)s);
        addmch(s, len);
        s += len;
        i += width;
    }
}

void addnstr_sup(char* s, int n)
{
    int i;
    int len, width;

    for (i = 0; *s != '\0';) {
        width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        len = wtf_len((wc_uchar*)s);
        addmch(s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        addch(' ');
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

void bell()
{
    write1(7);
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

void flush_tty(void)
{
    if (ttyf)
        fflush(ttyf);
}
