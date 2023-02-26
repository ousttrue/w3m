/* $Id: terms.c,v 1.63 2010/08/20 09:34:47 htrb Exp $ */
/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "tty.h"
// #include <termios.h>
#include "config.h"
// #include <errno.h>
// #include <fcntl.h>
#include <signal.h>
// #include <stdio.h>
// #include <string.h>
// #include <sys/stat.h>
// #include <sys/time.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <unistd.h>
// #ifdef HAVE_SYS_SELECT_H
// #include <sys/select.h>
// #endif
#include "fm.h"
// #include "myctype.h"
// #include "terms.h"
// #include "image.h"
// #include <sys/ioctl.h>

static char *title_str = NULL;

// MySignalHandler reset_exit(SIGNAL_ARG), reset_error_exit(SIGNAL_ARG),
//     error_dump(SIGNAL_ARG);
// void setlinescols(void);

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#define MAX_LINE 200
#define MAX_COLUMN 400

/* Screen properties */
#define S_SCREENPROP 0x0f
#define S_NORMAL 0x00
#define S_STANDOUT 0x01
#define S_UNDERLINE 0x02
#define S_BOLD 0x04
#define S_EOL 0x08

/* Sort of Character */
#define C_WHICHCHAR 0xc0
#define C_ASCII 0x00
#define C_WCHAR1 0x40
#define C_WCHAR2 0x80
#define C_CTRL 0xc0

#define CHMODE(c) ((c)&C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define SETCH(var, ch, len)                                                  \
    ((var) = New_Reuse(char, (var), (len) + 1), strncpy((var), (ch), (len)), \
     (var)[len] = '\0')

/* Charactor Color */
#define COL_FCOLOR 0xf00
#define COL_FBLACK 0x800
#define COL_FRED 0x900
#define COL_FGREEN 0xa00
#define COL_FYELLOW 0xb00
#define COL_FBLUE 0xc00
#define COL_FMAGENTA 0xd00
#define COL_FCYAN 0xe00
#define COL_FWHITE 0xf00
#define COL_FTERM 0x000

#define S_COLORED 0xf00

/* Background Color */
#define COL_BCOLOR 0xf000
#define COL_BBLACK 0x8000
#define COL_BRED 0x9000
#define COL_BGREEN 0xa000
#define COL_BYELLOW 0xb000
#define COL_BBLUE 0xc000
#define COL_BMAGENTA 0xd000
#define COL_BCYAN 0xe000
#define COL_BWHITE 0xf000
#define COL_BTERM 0x0000

#define S_BCOLORED 0xf000

#define S_GRAPHICS 0x10

#define S_DIRTY 0x20

#define SETPROP(var, prop) (var = (((var)&S_DIRTY) | prop))

/* Line status */
#define L_DIRTY 0x01
#define L_UNUSED 0x02
#define L_NEED_CE 0x04
#define L_CLRTOEOL 0x08

#define ISDIRTY(d) ((d)&L_DIRTY)
#define ISUNUSED(d) ((d)&L_UNUSED)
#define NEED_CE(d) ((d)&L_NEED_CE)

typedef unsigned short l_prop;

typedef struct scline
{
    char **lineimage;
    l_prop *lineprop;
    short isdirty;
    short eol;
} Screen;

static char bp[1024], funcstr[256];

char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc, *T_so,
    *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me, *T_ti, *T_te,
    *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

int LINES, COLS;
#if defined(__CYGWIN__)
int LASTLINE;
#endif /* defined(__CYGWIN__) */

static int max_LINES = 0, max_COLS = 0;
static int tab_step = 8;
static int CurLine, CurColumn;
static Screen *ScreenElem = NULL, **ScreenImage = NULL;
static l_prop CurrentMode = 0;
static int graph_enabled = 0;

static char gcmap[96];

extern "C"
{
    extern int tgetent(char *, char *);
    extern int tgetnum(char *);
    extern int tgetflag(char *);
    extern char *tgetstr(char *, char **);
    extern char *tgoto(char *, int, int);
}
void clear();
void wrap();
void touch_line();
void touch_column(int);
void clrtoeol(void); /* conflicts with curs_clear(3)? */

void MOVE(int line, int column)
{
    tty::writestr(tgoto(T_cm, column, line));
}

#define W3M_TERM_INFO(name, title, mouse) name, title

auto XTERM_TITLE = "\033]0;w3m: %s\007";
auto SCREEN_TITLE = "\033k%s\033\134";

/* *INDENT-OFF* */
static struct w3m_term_info
{
    const char *term;
    const char *title_str;
} w3m_term_info_list[] = {
    {W3M_TERM_INFO("xterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("kterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("rxvt", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("Eterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("mlterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("screen", SCREEN_TITLE, 0)},
#ifdef __CYGWIN__
    {W3M_TERM_INFO("cygwin", CYGWIN_TITLE, (NEED_CYGWIN_ON | NEED_CYGWIN_OFF))},
#endif
    {W3M_TERM_INFO(NULL, NULL, 0)}};
#undef W3M_TERM_INFO
/* *INDENT-ON * */

void reset_tty(void)
{
    tty::writestr(T_op); /* turn off */
    tty::writestr(T_me);
    if (!Do_not_use_ti_te)
    {
        if (T_te && *T_te)
            tty::writestr(T_te);
        else
            tty::writestr(T_cl);
    }
    tty::writestr(T_se); /* reset terminal */
    tty::flush();

    tty::reset();
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

MySignalHandler reset_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler error_dump(SIGNAL_ARG)
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

static void setgraphchar(void)
{
    int c, i, n;

    for (c = 0; c < 96; c++)
        gcmap[c] = (char)(c + ' ');

    if (!T_ac)
        return;

    n = strlen(T_ac);
    for (i = 0; i < n - 1; i += 2)
    {
        c = (unsigned)T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            gcmap[c] = T_ac[i + 1];
    }
}

#define graphchar(c) \
    (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c))
#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

void getTCstr(void)
{
    char *ent;
    char *suc;
    char *pt = funcstr;
    int r;

    ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL)
    {
        fprintf(stderr, "TERM is not set\n");
        reset_error_exit(SIGNAL_ARGLIST);
    }

    r = tgetent(bp, ent);
    if (r != 1)
    {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        reset_error_exit(SIGNAL_ARGLIST);
    }

    GETSTR(T_ce, "ce"); /* clear to the end of line */
    GETSTR(T_cd, "cd"); /* clear to the end of display */
    GETSTR(T_kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(T_kr, "kr");
    if (tgetflag("bs"))
        T_kl = "\b"; /* cursor left */
    else
    {
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
#if defined(CYGWIN) && CYGWIN < 1
    /* for TERM=pcansi on MS-DOS prompt. */
    T_eA = "";
    T_as = "";
    T_ae = "";
    T_ac = "";
#endif /* CYGWIN */

    LINES = COLS = 0;
    setlinescols();
    setgraphchar();
}

void setlinescols(void)
{
    if (auto size = tty::get_lines_cols())
    {
        auto [LIENS, COLS] = *size;
    }

    char *p;
    int i;
    if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
        LINES = i;
    if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
        COLS = i;
    if (LINES <= 0)
        LINES = tgetnum("li"); /* number of line */
    if (COLS <= 0)
        COLS = tgetnum("co"); /* number of column */
    if (COLS > MAX_COLUMN)
        COLS = MAX_COLUMN;
    if (LINES > MAX_LINE)
        LINES = MAX_LINE;
#if defined(__CYGWIN__)
    LASTLINE = LINES - (isWinConsole == TERM_CYGWIN_RESERVE_IME ? 2 : 1);
#endif /* defined(__CYGWIN__) */
}

void setupscreen(void)
{
    int i;

    if (LINES + 1 > max_LINES)
    {
        max_LINES = LINES + 1;
        max_COLS = 0;
        ScreenElem = New_N(Screen, max_LINES);
        ScreenImage = New_N(Screen *, max_LINES);
    }
    if (COLS + 1 > max_COLS)
    {
        max_COLS = COLS + 1;
        for (i = 0; i < max_LINES; i++)
        {
            ScreenElem[i].lineimage = New_N(char *, max_COLS);
            bzero((void *)ScreenElem[i].lineimage, max_COLS * sizeof(char *));
            ScreenElem[i].lineprop = New_N(l_prop, max_COLS);
        }
    }
    for (i = 0; i < LINES; i++)
    {
        ScreenImage[i] = &ScreenElem[i];
        ScreenImage[i]->lineprop[0] = S_EOL;
        ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++)
    {
        ScreenElem[i].isdirty = L_UNUSED;
    }

    clear();
}

/*
 * Screen initialize
 */
int initscr(void)
{
    tty::set();
    set_int();
    getTCstr();
    if (T_ti && !Do_not_use_ti_te)
        tty::writestr(T_ti);
    setupscreen();
    return 0;
}

void move(int line, int column)
{
    if (line >= 0 && line < LINES)
        CurLine = line;
    if (column >= 0 && column < COLS)
        CurColumn = column;
}

#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)

static int need_redraw(char *c1, l_prop pr1, char *c2, l_prop pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

#define SPACE " "

void addch(char c)
{
    addmch(&c, 1);
}

void addmch(char *pc, size_t len)
{
    l_prop *pr;
    int dest, i;
    static Str tmp = NULL;
    char **p;
    char c = *pc;
    int width = wtf_width((wc_uchar *)pc);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;

    if (CurColumn == COLS)
        wrap();
    if (CurColumn >= COLS)
        return;
    p = ScreenImage[CurLine]->lineimage;
    pr = ScreenImage[CurLine]->lineprop;

    if (pr[CurColumn] & S_EOL)
    {
        if (c == ' ' && !(CurrentMode & M_SPACE))
        {
            CurColumn++;
            return;
        }
        for (i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--)
        {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
        }
    }

    if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
        SETCHMODE(CurrentMode, C_CTRL);
    else if (len > 1)
        SETCHMODE(CurrentMode, C_WCHAR1);
    else if (!IS_CNTRL(c))
        SETCHMODE(CurrentMode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = CurColumn + width - 1;
    if (i < COLS &&
        (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], pc, CurrentMode)) ||
         ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE))))
    {
        touch_line();
        i++;
        if (i < COLS)
        {
            touch_column(i);
            if (pr[i] & S_EOL)
            {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            }
            else
            {
                for (i++; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
                    touch_column(i);
            }
        }
    }

    if (CurColumn + width > COLS)
    {
        touch_line();
        for (i = CurColumn; i < COLS; i++)
        {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
        }
        wrap();
        if (CurColumn + width > COLS)
            return;
        p = ScreenImage[CurLine]->lineimage;
        pr = ScreenImage[CurLine]->lineprop;
    }
    if (CHMODE(pr[CurColumn]) == C_WCHAR2)
    {
        touch_line();
        for (i = CurColumn - 1; i >= 0; i--)
        {
            l_prop l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(CurrentMode) != C_CTRL)
    {
        if (need_redraw(p[CurColumn], pr[CurColumn], pc, CurrentMode))
        {
            SETCH(p[CurColumn], pc, len);
            SETPROP(pr[CurColumn], CurrentMode);
            touch_line();
            touch_column(CurColumn);
            SETCHMODE(CurrentMode, C_WCHAR2);
            for (i = CurColumn + 1; i < CurColumn + width; i++)
            {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                touch_column(i);
            }
            for (; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
            {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                touch_column(i);
            }
        }
        CurColumn += width;
    }
    else if (c == '\t')
    {
        dest = (CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= COLS)
        {
            wrap();
            touch_line();
            dest = tab_step;
            p = ScreenImage[CurLine]->lineimage;
            pr = ScreenImage[CurLine]->lineprop;
        }
        for (i = CurColumn; i < dest; i++)
        {
            if (need_redraw(p[i], pr[i], SPACE, CurrentMode))
            {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], CurrentMode);
                touch_line();
                touch_column(i);
            }
        }
        CurColumn = i;
    }
    else if (c == '\n')
    {
        wrap();
    }
    else if (c == '\r')
    { /* Carriage return */
        CurColumn = 0;
    }
    else if (c == '\b' && CurColumn > 0)
    { /* Backspace */
        CurColumn--;
        while (CurColumn > 0 && CHMODE(pr[CurColumn]) == C_WCHAR2)
            CurColumn--;
    }
}

void wrap(void)
{
    if (CurLine == LASTLINE)
        return;
    CurLine++;
    CurColumn = 0;
}

void touch_column(int col)
{
    if (col >= 0 && col < COLS)
        ScreenImage[CurLine]->lineprop[col] |= S_DIRTY;
}

void touch_line(void)
{
    if (!(ScreenImage[CurLine]->isdirty & L_DIRTY))
    {
        int i;
        for (i = 0; i < COLS; i++)
            ScreenImage[CurLine]->lineprop[i] &= ~S_DIRTY;
        ScreenImage[CurLine]->isdirty |= L_DIRTY;
    }
}

void standout(void)
{
    CurrentMode |= S_STANDOUT;
}

void standend(void)
{
    CurrentMode &= ~S_STANDOUT;
}

void toggle_stand(void)
{
    int i;
    l_prop *pr = ScreenImage[CurLine]->lineprop;
    pr[CurColumn] ^= S_STANDOUT;
    if (CHMODE(pr[CurColumn]) != C_WCHAR2)
    {
        for (i = CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void bold(void)
{
    CurrentMode |= S_BOLD;
}

void boldend(void)
{
    CurrentMode &= ~S_BOLD;
}

void underline(void)
{
    CurrentMode |= S_UNDERLINE;
}

void underlineend(void)
{
    CurrentMode &= ~S_UNDERLINE;
}

void graphstart(void)
{
    CurrentMode |= S_GRAPHICS;
}

void graphend(void)
{
    CurrentMode &= ~S_GRAPHICS;
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return T_as[0] != 0 && T_ae[0] != 0 && T_ac[0] != 0;
}

void setfcolor(int color)
{
    CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 8);
}

static char *color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + 30);
    return seqbuf;
}

void setbcolor(int color)
{
    CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 12);
}

static char *bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#define M_MEND \
    (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS)
void refresh(void)
{
    int line, col, pcol;
    int pline = CurLine;
    int moved = RF_NEED_TO_MOVE;
    char **pc;
    l_prop *pr, mode = 0;
    l_prop color = COL_FTERM;
    l_prop bcolor = COL_BTERM;
    short *dirty;

    wc_putc_init(InnerCharset, DisplayCharset);
    for (line = 0; line <= LASTLINE; line++)
    {
        dirty = &ScreenImage[line]->isdirty;
        if (*dirty & L_DIRTY)
        {
            *dirty &= ~L_DIRTY;
            pc = ScreenImage[line]->lineimage;
            pr = ScreenImage[line]->lineprop;
            for (col = 0; col < COLS && !(pr[col] & S_EOL); col++)
            {
                if (*dirty & L_NEED_CE && col >= ScreenImage[line]->eol)
                {
                    if (need_redraw(pc[col], pr[col], SPACE, 0))
                        break;
                }
                else
                {
                    if (pr[col] & S_DIRTY)
                        break;
                }
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL))
            {
                pcol = ScreenImage[line]->eol;
                if (pcol >= COLS)
                {
                    *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
                    pcol = col;
                }
            }
            else
            {
                pcol = col;
            }
            if (line < LINES - 2 && pline == line - 1 && pcol == 0)
            {
                switch (moved)
                {
                case RF_NEED_TO_MOVE:
                    MOVE(line, 0);
                    moved = RF_CR_OK;
                    break;
                case RF_CR_OK:
                    tty::write1('\n');
                    tty::write1('\r');
                    break;
                case RF_NONEED_TO_MOVE:
                    moved = RF_CR_OK;
                    break;
                }
            }
            else
            {
                MOVE(line, pcol);
                moved = RF_CR_OK;
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL))
            {
                tty::writestr(T_ce);
                if (col != pcol)
                    MOVE(line, col);
            }
            pline = line;
            pcol = col;
            for (; col < COLS; col++)
            {
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
#ifdef __CYGWIN__
                if (isWinConsole)
#endif
                    if (line == LINES - 1 && col == COLS - 1)
                        break;
#endif /* !defined(USE_BG_COLOR) || defined(__CYGWIN__) */
                if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) ||
                    (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) ||
                    (!(pr[col] & S_BOLD) && (mode & S_BOLD)) ||
                    (!(pr[col] & S_COLORED) && (mode & S_COLORED)) ||
                    (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED)) ||
                    (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS)))
                {
                    if ((mode & S_COLORED) || (mode & S_BCOLORED))
                        tty::writestr(T_op);
                    if (mode & S_GRAPHICS)
                        tty::writestr(T_ae);
                    tty::writestr(T_me);
                    mode &= ~M_MEND;
                }
                if ((*dirty & L_NEED_CE && col >= ScreenImage[line]->eol)
                        ? need_redraw(pc[col], pr[col], SPACE, 0)
                        : (pr[col] & S_DIRTY))
                {
                    if (pcol == col - 1)
                        tty::writestr(T_nd);
                    else if (pcol != col)
                        MOVE(line, col);

                    if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT))
                    {
                        tty::writestr(T_so);
                        mode |= S_STANDOUT;
                    }
                    if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE))
                    {
                        tty::writestr(T_us);
                        mode |= S_UNDERLINE;
                    }
                    if ((pr[col] & S_BOLD) && !(mode & S_BOLD))
                    {
                        tty::writestr(T_md);
                        mode |= S_BOLD;
                    }
                    if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR)
                    {
                        color = (pr[col] & COL_FCOLOR);
                        mode = ((mode & ~COL_FCOLOR) | color);
                        tty::writestr(color_seq(color));
                    }
                    if ((pr[col] & S_BCOLORED) && (pr[col] ^ mode) & COL_BCOLOR)
                    {
                        bcolor = (pr[col] & COL_BCOLOR);
                        mode = ((mode & ~COL_BCOLOR) | bcolor);
                        tty::writestr(bcolor_seq(bcolor));
                    }
                    if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS))
                    {
                        wc_putc_end(tty::get_file());
                        if (!graph_enabled)
                        {
                            graph_enabled = 1;
                            tty::writestr(T_eA);
                        }
                        tty::writestr(T_as);
                        mode |= S_GRAPHICS;
                    }
                    if (pr[col] & S_GRAPHICS)
                        tty::write1(graphchar(*pc[col]));
                    else if (CHMODE(pr[col]) != C_WCHAR2)
                        wc_putc(pc[col], tty::get_file());
                    pcol = col + 1;
                }
            }
            if (col == COLS)
                moved = RF_NEED_TO_MOVE;
            for (; col < COLS && !(pr[col] & S_EOL); col++)
                pr[col] |= S_EOL;
        }
        *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
        if (mode & M_MEND)
        {
            if (mode & (S_COLORED | S_BCOLORED))
                tty::writestr(T_op);
            if (mode & S_GRAPHICS)
            {
                tty::writestr(T_ae);
                wc_putc_clear_status();
            }
            tty::writestr(T_me);
            mode &= ~M_MEND;
        }
    }
    wc_putc_end(tty::get_file());
    MOVE(CurLine, CurColumn);
    tty::flush();
}

void clear(void)
{
    int i, j;
    l_prop *p;
    tty::writestr(T_cl);
    move(0, 0);
    for (i = 0; i < LINES; i++)
    {
        ScreenImage[i]->isdirty = 0;
        p = ScreenImage[i]->lineprop;
        for (j = 0; j < COLS; j++)
        {
            p[j] = S_EOL;
        }
    }
    CurrentMode = C_ASCII;
}

static void scroll_raw(void)
{ /* raw scroll */
    MOVE(LINES - 1, 0);
    tty::write1('\n');
}

void scroll(int n)
{ /* scroll up */
    int cli = CurLine, cco = CurColumn;
    Screen *t;
    int i, j, k;

    i = LINES;
    j = n;
    do
    {
        k = j;
        j = i % k;
        i = k;
    } while (j);
    do
    {
        k--;
        i = k;
        j = (i + n) % LINES;
        t = ScreenImage[k];
        while (j != k)
        {
            ScreenImage[i] = ScreenImage[j];
            i = j;
            j = (i + n) % LINES;
        }
        ScreenImage[i] = t;
    } while (k);

    for (i = 0; i < n; i++)
    {
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
    Screen *t;
    int i, j, k;

    i = LINES;
    j = n;
    do
    {
        k = j;
        j = i % k;
        i = k;
    } while (j);
    do
    {
        k--;
        i = k;
        j = (LINES + i - n) % LINES;
        t = ScreenImage[k];
        while (j != k)
        {
            ScreenImage[i] = ScreenImage[j];
            i = j;
            j = (LINES + i - n) % LINES;
        }
        ScreenImage[i] = t;
    } while (k);
    if (T_sr && *T_sr)
    {
        MOVE(0, 0);
        for (i = 0; i < n; i++)
        {
            t = ScreenImage[i];
            t->isdirty = 0;
            for (j = 0; j < COLS; j++)
                t->lineprop[j] = S_EOL;
            tty::writestr(T_sr);
        }
        move(cli, cco);
    }
    else
    {
        for (i = 0; i < LINES; i++)
        {
            t = ScreenImage[i];
            t->isdirty |= L_DIRTY | L_NEED_CE;
            for (j = 0; j < COLS; j++)
            {
                t->lineprop[j] |= S_DIRTY;
            }
        }
    }
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void)
{ /* Clear to the end of line */
    int i;
    l_prop *lprop = ScreenImage[CurLine]->lineprop;

    if (lprop[CurColumn] & S_EOL)
        return;

    if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) ||
        ScreenImage[CurLine]->eol > CurColumn)
        ScreenImage[CurLine]->eol = CurColumn;

    ScreenImage[CurLine]->isdirty |= L_CLRTOEOL;
    touch_line();
    for (i = CurColumn; i < COLS && !(lprop[i] & S_EOL); i++)
    {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

void clrtoeol_with_bcolor(void)
{
    int i, cli, cco;
    l_prop pr;

    if (!(CurrentMode & S_BCOLORED))
    {
        clrtoeol();
        return;
    }
    cli = CurLine;
    cco = CurColumn;
    pr = CurrentMode;
    CurrentMode = (CurrentMode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (i = CurColumn; i < COLS; i++)
        addch(' ');
    move(cli, cco);
    CurrentMode = pr;
}

void clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

void clrtobot_eol(void (*clrtoeol)())
{
    int l, c;

    l = CurLine;
    c = CurColumn;
    (*clrtoeol)();
    CurColumn = 0;
    CurLine++;
    for (; CurLine < LINES; CurLine++)
        (*clrtoeol)();
    CurLine = l;
    CurColumn = c;
}

void clrtobot(void)
{
    clrtobot_eol(clrtoeol);
}

void clrtobotx(void)
{
    clrtobot_eol(clrtoeolx);
}

void addstr(char *s)
{
    int len;

    while (*s != '\0')
    {
        len = wtf_len((wc_uchar *)s);
        addmch(s, len);
        s += len;
    }
}

void addnstr(char *s, int n)
{
    int i;
    int len, width;

    for (i = 0; *s != '\0';)
    {
        width = wtf_width((wc_uchar *)s);
        if (i + width > n)
            break;
        len = wtf_len((wc_uchar *)s);
        addmch(s, len);
        s += len;
        i += width;
    }
}

void addnstr_sup(char *s, int n)
{
    int i;
    int len, width;

    for (i = 0; *s != '\0';)
    {
        width = wtf_width((wc_uchar *)s);
        if (i + width > n)
            break;
        len = wtf_len((wc_uchar *)s);
        addmch(s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        addch(' ');
}

void term_title(char *s)
{
    if (!fmInitialized)
        return;
    if (title_str != NULL)
    {
#ifdef __CYGWIN__
        if (isLocalConsole && title_str == CYGWIN_TITLE)
        {
            Str buff;
            buff = Sprintf(title_str, s);
            if (buff->length > 1024)
            {
                Strtruncate(buff, 1024);
            }
            SetConsoleTitle(buff->ptr);
        }
        else if (isLocalConsole || !isWinConsole)
#endif
            fprintf(tty::get_file(), title_str, s);
    }
}

void bell(void)
{
    tty::write1(7);
}

void touch_cursor()
{
    int i;
    touch_line();
    for (i = CurColumn; i >= 0; i--)
    {
        touch_column(i);
        if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = CurColumn + 1; i < COLS; i++)
    {
        if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        touch_column(i);
    }
}
