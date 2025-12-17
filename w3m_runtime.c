#include "w3m_runtime.h"
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
    .fmInitialized = false,
    .Do_not_use_ti_te = false,
};
struct Runtime* getRuntime()
{
    return &g_runtime;
}
bool fmInitialized() { return g_runtime.fmInitialized; }
#define MAXIMUM_COLS 1024
void tty_set_cols(int cols)
{
    g_runtime.cols = cols;
    if (g_runtime.cols > MAXIMUM_COLS) {
        g_runtime.cols = MAXIMUM_COLS;
    }
}

void enterRawMode(void)
{
    if (!g_runtime.fmInitialized) {
        initscr();
        term_raw();
        term_noecho();
        initImage();
    }
    g_runtime.fmInitialized = TRUE;
}

void exitRawMode(void)
{
    if (g_runtime.fmInitialized) {
        move(LASTLINE(), 0);
        clrtoeolx();
        refresh();
        loadImage(NULL, IMG_FLAG_STOP);
        reset_tty();
        g_runtime.fmInitialized = FALSE;
    }
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval)
{
    reset_tty();
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
    reset_tty();
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

void setlinescols(void)
{
    struct winsize wins;
    int i = ioctl(g_runtime.tty_input, TIOCGWINSZ, &wins);
    if (i >= 0 && wins.ws_row != 0 && wins.ws_col != 0) {
        g_runtime.lines = wins.ws_row;
        g_runtime.cols = wins.ws_col;
    }
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
    // stdin
    g_runtime.tty_input = 0;
    tcgetattr(g_runtime.tty_input, &d_ioval);

    getTCstr();
}

char* ttyname_tty(void)
{
    return ttyname(g_runtime.tty_input);
}

void reset_tty(void)
{
    writestr(g_runtime.T_op); /* turn off */
    writestr(g_runtime.T_me);
    if (!g_runtime.Do_not_use_ti_te) {
        if (g_runtime.T_te && *g_runtime.T_te)
            writestr(g_runtime.T_te);
        else
            writestr(g_runtime.T_cl);
    }
    writestr(g_runtime.T_se); /* reset terminal */
    flush_tty();
    tcsetattr(g_runtime.tty_input, TCSANOW, &d_ioval);
}

void ttymode_set(int mode, int imode)
{
    struct termios ioval;
    tcgetattr(g_runtime.tty_input, &ioval);
    ioval.c_lflag |= mode;
    ioval.c_iflag |= imode;
    while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while set %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void ttymode_reset(int mode, int imode)
{
    struct termios ioval;
    tcgetattr(g_runtime.tty_input, &ioval);
    ioval.c_lflag &= ~mode;
    ioval.c_iflag &= ~imode;
    while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred while reset %x: errno=%d\n", mode, errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

void set_cc(int spec, int val)
{
    struct termios ioval;
    tcgetattr(g_runtime.tty_input, &ioval);
    ioval.c_cc[spec] = val;
    while (tcsetattr(g_runtime.tty_input, TCSANOW, &ioval) == -1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        printf("Error occurred: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
}

char getch(void)
{
    char c;

    while (
        read(getRuntime()->tty_input, &c, 1)
        < (int)1) {
        if (errno == EINTR || errno == EAGAIN)
            continue;
        /* error happend on read(2) */
        quitfm();
        break; /* unreachable */
    }
    return c;
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

int initscr(void)
{
    set_int();
    if (g_runtime.T_ti && !g_runtime.Do_not_use_ti_te)
        writestr(g_runtime.T_ti);
    setupscreen();
    return 0;
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return g_runtime.T_as[0] != 0 && g_runtime.T_ae[0] != 0 && g_runtime.T_ac[0] != 0;
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

#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
void term_raw(void)
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
