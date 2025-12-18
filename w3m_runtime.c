#include "w3m_runtime.h"
#include "fm.h"
#include "tab.h"
#include "image.h"
#include "terms.h"
#include "config.h"
#include "indep.h"
#include "myctype.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <errno.h>
#include <termcap.h>
#include <unistd.h>

static struct termios d_ioval;

// rc
char UseGraphicChar = GRAPHIC_CHAR_CHARSET;

struct Runtime g_runtime = {
    .lines = 0,
    .cols = 0,
    .Do_not_use_ti_te = false,

    .CurrentTab = 0,
    .FirstTab = 0,
    .LastTab = 0,
    .nTab = 0,
};
struct Runtime* getRuntime()
{
    return &g_runtime;
}
struct TabBuffer* CurrentTab()
{
    return g_runtime.CurrentTab;
}
struct TabBuffer* FirstTab()
{
    return g_runtime.FirstTab;
}
struct TabBuffer* LastTab()
{
    return g_runtime.LastTab;
}
int nTab()
{
    return g_runtime.nTab;
}

#define MAXIMUM_COLS 1024
void tty_set_cols(int cols)
{
    g_runtime.cols = cols;
    if (g_runtime.cols > MAXIMUM_COLS) {
        g_runtime.cols = MAXIMUM_COLS;
    }
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval)
{
    exitRawMode();
    w3m_exit(rval);
    SIGNAL_RETURN;
}

MySignalHandler reset_error_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

MySignalHandler
reset_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler
error_dump(SIGNAL_ARG)
{
    mySignal(SIGIOT, SIG_DFL);
    exitRawMode();
    abort();
    SIGNAL_RETURN;
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

    for (int c = 0; c < 96; c++)
        g_runtime.gcmap[c] = (char)(c + ' ');

    if (!getRuntime()->T_ac)
        return;

    int n = strlen(getRuntime()->T_ac);
    for (int i = 0; i < n - 1; i += 2) {
        int c = (unsigned)getRuntime()->T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            g_runtime.gcmap[c] = getRuntime()->T_ac[i + 1];
    }
}

char graphchar(char c)
{
    return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? g_runtime.gcmap[(c) - ' '] : (c));
}

#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

static char bp[1024], funcstr[256];

static void getTCstr(void)
{
    char* ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        reset_error_exit(SIGNAL_ARGLIST);
    }

    int r = tgetent(bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        reset_error_exit(SIGNAL_ARGLIST);
    }

    char* suc;
    char* pt = funcstr;
    GETSTR(g_runtime.T_ce, "ce"); /* clear to the end of line */
    GETSTR(g_runtime.T_cd, "cd"); /* clear to the end of display */
    GETSTR(g_runtime.T_kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(g_runtime.T_kr, "kr");
    if (tgetflag("bs"))
        g_runtime.T_kl = "\b"; /* cursor left */
    else {
        GETSTR(g_runtime.T_kl, "le");
        if (suc == NULL)
            GETSTR(g_runtime.T_kl, "kb");
        if (suc == NULL)
            GETSTR(g_runtime.T_kl, "kl");
    }
    GETSTR(g_runtime.T_cr, "cr"); /* carriage return */
    GETSTR(g_runtime.T_ta, "ta"); /* tab */
    GETSTR(g_runtime.T_sc, "sc"); /* save cursor */
    GETSTR(g_runtime.T_rc, "rc"); /* restore cursor */
    GETSTR(g_runtime.T_so, "so"); /* standout mode */
    GETSTR(g_runtime.T_se, "se"); /* standout mode end */
    GETSTR(g_runtime.T_us, "us"); /* underline mode */
    GETSTR(g_runtime.T_ue, "ue"); /* underline mode end */
    GETSTR(g_runtime.T_md, "md"); /* bold mode */
    GETSTR(g_runtime.T_me, "me"); /* bold mode end */
    GETSTR(g_runtime.T_cl, "cl"); /* clear screen */
    GETSTR(g_runtime.T_cm, "cm"); /* cursor move */
    GETSTR(g_runtime.T_al, "al"); /* append line */
    GETSTR(g_runtime.T_sr, "sr"); /* scroll reverse */
    GETSTR(g_runtime.T_ti, "ti"); /* terminal init */
    GETSTR(g_runtime.T_te, "te"); /* terminal end */
    GETSTR(g_runtime.T_nd, "nd"); /* move right one space */
    GETSTR(g_runtime.T_eA, "eA"); /* enable alternative charset */
    GETSTR(g_runtime.T_as, "as"); /* alternative (graphic) charset start */
    GETSTR(g_runtime.T_ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(g_runtime.T_ac, "ac"); /* graphics charset pairs */
    GETSTR(g_runtime.T_op, "op"); /* set default color pair to its original value */
#if defined(CYGWIN) && CYGWIN < 1
    /* for TERM=pcansi on MS-DOS prompt. */
#if 0
    T_eA = "";
    T_as = "\033[12m";
    T_ae = "\033[10m";
    T_ac = "l\001k\002m\003j\004x\005q\006n\020a\024v\025w\026u\027t\031";
#endif
    T_eA = "";
    T_as = "";
    T_ae = "";
    T_ac = "";
#endif /* CYGWIN */

    setlinescols();
    setgraphchar();
}

void init_tty()
{
    getTCstr();
}

char* ttyname_tty(void)
{
    return ttyname(0);
    // g_runtime.tty_input);
}

static void
skip_escseq(void)
{
    int c = getch();
    if (c == '[' || c == 'O') {
        c = getch();
        while (IS_DIGIT(c))
            c = getch();
    }
}

// int sleep_till_anykey(int sec, bool purge)
// {
//     struct termios ioval;
//     tcgetattr(getRuntime()->tty_input, &ioval);
//     term_raw();
//
//     struct timeval tim;
//     tim.tv_sec = sec;
//     tim.tv_usec = 0;
//
//     fd_set rfd;
//     FD_ZERO(&rfd);
//     FD_SET(getRuntime()->tty_input, &rfd);
//
//     int ret = select(getRuntime()->tty_input + 1, &rfd, 0, 0, &tim);
//     if (ret > 0 && purge) {
//         int c = getch();
//         if (c == ESC_CODE)
//             skip_escseq();
//     }
//     int er = tcsetattr(getRuntime()->tty_input, TCSANOW, &ioval);
//     if (er == -1) {
//         printf("Error occurred: errno=%d\n", errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
//     return ret;
// }

void tty_MOVE(int line, int column)
{
    writestr(tgoto(g_runtime.T_cm, column, line));
}

void initscr(void)
{
    set_int();
    if (g_runtime.T_ti && !g_runtime.Do_not_use_ti_te)
        writestr(g_runtime.T_ti);
    setupscreen();
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return g_runtime.T_as[0] != 0 && g_runtime.T_ae[0] != 0 && g_runtime.T_ac[0] != 0;
}

static const char* title_str = NULL;

void term_title(const char* s)
{
    if (!fmInitialized())
        return;
    if (title_str != NULL) {
        // fprintf(tty_output_f, title_str, s);
    }
}

void bell(void)
{
    write1(7);
}

//
// tab
//
void _newT(void)
{
    struct TabBuffer* tag = newTab();
    if (!tag)
        return;

    Buffer* buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    buf->nextBuffer = NULL;
    for (int i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;
    tag->firstBuffer = tag->currentBuffer = buf;

    tag->nextTab = g_runtime.CurrentTab->nextTab;
    tag->prevTab = g_runtime.CurrentTab;
    if (g_runtime.CurrentTab->nextTab)
        g_runtime.CurrentTab->nextTab->prevTab = tag;
    else
        g_runtime.LastTab = tag;
    g_runtime.CurrentTab->nextTab = tag;
    g_runtime.CurrentTab = tag;
    g_runtime.nTab++;
}

void tabs_prepare()
{
    g_runtime.CurrentTab = g_runtime.LastTab;
    if (!g_runtime.FirstTab) {
        g_runtime.FirstTab = g_runtime.LastTab = g_runtime.CurrentTab = newTab();
        g_runtime.nTab = 1;
    }
}

void calcTabPos(void)
{
    struct TabBuffer* tab;
    int lcol = 0, rcol = 0, col;
    int n1, n2, na, nx, ny, ix, iy;

    if (nTab <= 0)
        return;
    n1 = (TTY_COLS() - rcol - lcol) / TabCols;
    if (n1 >= g_runtime.nTab) {
        n2 = 1;
        ny = 1;
    } else {
        if (n1 < 0)
            n1 = 0;
        n2 = TTY_COLS() / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (g_runtime.nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - g_runtime.nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = g_runtime.FirstTab;
    for (iy = 0; iy < ny && tab; iy++) {
        if (iy == 0) {
            nx = n1;
            col = TTY_COLS() - rcol - lcol;
        } else {
            nx = n2 - (na - g_runtime.nTab + (iy - 1)) / (ny - 1);
            col = TTY_COLS();
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;
            tab->y = iy;
            if (iy == 0) {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
}
