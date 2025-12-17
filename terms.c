/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "terms.h"
#include "w3m_runtime.h"
#include "fm.h"
#include "config.h"
#include "myctype.h"
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ioctl.h>

static char* title_str = NULL;

MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);

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
#ifdef USE_M17N
#define C_WCHAR1 0x40
#define C_WCHAR2 0x80
#endif
#define C_CTRL 0xc0

#define CHMODE(c) ((c) & C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#ifdef USE_M17N
#define SETCH(var, ch, len) ((var) = New_Reuse(char, (var), (len) + 1), \
    strncpy((var), (ch), (len + 1)))
#else
#define SETCH(var, ch, len) ((var) = (ch))
#endif

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

#ifdef USE_BG_COLOR
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
#endif /* USE_BG_COLOR */

#define S_GRAPHICS 0x10

#define S_DIRTY 0x20

#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))

/* Line status */
#define L_DIRTY 0x01
#define L_UNUSED 0x02
#define L_NEED_CE 0x04
#define L_CLRTOEOL 0x08

#define ISDIRTY(d) ((d) & L_DIRTY)
#define ISUNUSED(d) ((d) & L_UNUSED)
#define NEED_CE(d) ((d) & L_NEED_CE)

typedef unsigned short l_prop;

typedef struct scline {
#ifdef USE_M17N
    char** lineimage;
#else
    char* lineimage;
#endif
    l_prop* lineprop;
    short isdirty;
    short eol;
} Screen;

static int max_LINES = 0, max_COLS = 0;
static int tab_step = 8;
static int CurLine, CurColumn;
static Screen *ScreenElem = NULL, **ScreenImage = NULL;
static l_prop CurrentMode = 0;
static int graph_enabled = 0;

extern int tgetent(char*, char*);
extern int tgetnum(char*);
extern int tgetflag(char*);
extern char* tgetstr(char*, char**);
extern char* tgoto(char*, int, int);
extern int tputs(char*, int, int (*)(char));
void clear(void), wrap(void), touch_line(void), touch_column(int);
void clrtoeol(void); /* conflicts with curs_clear(3)? */

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

void setupscreen(void)
{
    int i;

    if (getRuntime()->lines + 1 > max_LINES) {
        max_LINES = getRuntime()->lines + 1;
        max_COLS = 0;
        ScreenElem = New_N(Screen, max_LINES);
        ScreenImage = New_N(Screen*, max_LINES);
    }
    if (getRuntime()->cols + 1 > max_COLS) {
        max_COLS = getRuntime()->cols + 1;
        for (i = 0; i < max_LINES; i++) {
#ifdef USE_M17N
            ScreenElem[i].lineimage = New_N(char*, max_COLS);
            bzero((void*)ScreenElem[i].lineimage, max_COLS * sizeof(char*));
#else
            ScreenElem[i].lineimage = New_N(char, max_COLS);
#endif
            ScreenElem[i].lineprop = New_N(l_prop, max_COLS);
        }
    }
    for (i = 0; i < getRuntime()->lines; i++) {
        ScreenImage[i] = &ScreenElem[i];
        ScreenImage[i]->lineprop[0] = S_EOL;
        ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++) {
        ScreenElem[i].isdirty = L_UNUSED;
    }

    clear();
}

/*
 * Screen initialize
 */
void move(int line, int column)
{
    if (line >= 0 && line < getRuntime()->lines)
        CurLine = line;
    if (column >= 0 && column < getRuntime()->cols)
        CurColumn = column;
}

#ifdef USE_BG_COLOR
#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)
#else /* not USE_BG_COLOR */
#define M_SPACE (S_SCREENPROP | S_COLORED | S_GRAPHICS)
#endif /* not USE_BG_COLOR */

static int
#ifdef USE_M17N
need_redraw(char* c1, l_prop pr1, char* c2, l_prop pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
#else
need_redraw(char c1, l_prop pr1, char c2, l_prop pr2)
{
    if (c1 != c2)
        return 1;
    if (c1 == ' ')
#endif
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

#ifdef USE_M17N
#define SPACE " "
#else
#define SPACE ' '
#endif

#ifdef USE_M17N
void addch(char c)
{
    addmch(&c, 1);
}

void addmch(char* pc, size_t len)
#else
void addch(char pc)
#endif
{
    l_prop* pr;
    int dest, i;
#ifdef USE_M17N
    static Str tmp = NULL;
    char** p;
    char c = *pc;
    int width = wtf_width((wc_uchar*)pc);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;
#else
    char* p;
    char c = pc;
#endif

    if (CurColumn == getRuntime()->cols)
        wrap();
    if (CurColumn >= getRuntime()->cols)
        return;
    p = ScreenImage[CurLine]->lineimage;
    pr = ScreenImage[CurLine]->lineprop;

#ifndef USE_M17N
    /* Eliminate unprintables according to * iso-8859-*.
     * Particularly 0x96 messes up T.Dickey's * (xfree-)xterm */
    if (IS_INTSPACE(c))
        c = ' ';
#endif

    if (pr[CurColumn] & S_EOL) {
        if (c == ' ' && !(CurrentMode & M_SPACE)) {
            CurColumn++;
            return;
        }
        for (i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
        }
    }

    if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
        SETCHMODE(CurrentMode, C_CTRL);
#ifdef USE_M17N
    else if (len > 1)
        SETCHMODE(CurrentMode, C_WCHAR1);
#endif
    else if (!IS_CNTRL(c))
        SETCHMODE(CurrentMode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
#ifdef USE_M17N
    i = CurColumn + width - 1;
#else
    i = CurColumn;
#endif
    if (i < getRuntime()->cols && (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], pc, CurrentMode)) || ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
        touch_line();
        i++;
        if (i < getRuntime()->cols) {
            touch_column(i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            }
#ifdef USE_M17N
            else {
                for (i++; i < getRuntime()->cols && CHMODE(pr[i]) == C_WCHAR2; i++)
                    touch_column(i);
            }
#endif
        }
    }

#ifdef USE_M17N
    if (CurColumn + width > getRuntime()->cols) {
        touch_line();
        for (i = CurColumn; i < getRuntime()->cols; i++) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
        }
        wrap();
        if (CurColumn + width > getRuntime()->cols)
            return;
        p = ScreenImage[CurLine]->lineimage;
        pr = ScreenImage[CurLine]->lineprop;
    }
    if (CHMODE(pr[CurColumn]) == C_WCHAR2) {
        touch_line();
        for (i = CurColumn - 1; i >= 0; i--) {
            l_prop l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
#endif
    if (CHMODE(CurrentMode) != C_CTRL) {
        if (need_redraw(p[CurColumn], pr[CurColumn], pc, CurrentMode)) {
            SETCH(p[CurColumn], pc, len);
            SETPROP(pr[CurColumn], CurrentMode);
            touch_line();
            touch_column(CurColumn);
#ifdef USE_M17N
            SETCHMODE(CurrentMode, C_WCHAR2);
            for (i = CurColumn + 1; i < CurColumn + width; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                touch_column(i);
            }
            for (; i < getRuntime()->cols && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                touch_column(i);
            }
        }
        CurColumn += width;
#else
        }
        CurColumn++;
#endif
    } else if (c == '\t') {
        dest = (CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= getRuntime()->cols) {
            wrap();
            touch_line();
            dest = tab_step;
            p = ScreenImage[CurLine]->lineimage;
            pr = ScreenImage[CurLine]->lineprop;
        }
        for (i = CurColumn; i < dest; i++) {
            if (need_redraw(p[i], pr[i], SPACE, CurrentMode)) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], CurrentMode);
                touch_line();
                touch_column(i);
            }
        }
        CurColumn = i;
    } else if (c == '\n') {
        wrap();
    } else if (c == '\r') { /* Carriage return */
        CurColumn = 0;
    } else if (c == '\b' && CurColumn > 0) { /* Backspace */
        CurColumn--;
#ifdef USE_M17N
        while (CurColumn > 0 && CHMODE(pr[CurColumn]) == C_WCHAR2)
            CurColumn--;
#endif
    }
}

void wrap(void)
{
    if (CurLine == LASTLINE())
        return;
    CurLine++;
    CurColumn = 0;
}

void touch_column(int col)
{
    if (col >= 0 && col < getRuntime()->cols)
        ScreenImage[CurLine]->lineprop[col] |= S_DIRTY;
}

void touch_line(void)
{
    if (!(ScreenImage[CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < getRuntime()->cols; i++)
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
#ifdef USE_M17N
    int i;
#endif
    l_prop* pr = ScreenImage[CurLine]->lineprop;
    pr[CurColumn] ^= S_STANDOUT;
#ifdef USE_M17N
    if (CHMODE(pr[CurColumn]) != C_WCHAR2) {
        for (i = CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
#endif
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

void setfcolor(int color)
{
    CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 8);
}

static char*
color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    return seqbuf;
}

#ifdef USE_BG_COLOR
void setbcolor(int color)
{
    CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 12);
}

static char*
bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}
#endif /* USE_BG_COLOR */

#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#ifdef USE_BG_COLOR
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS)
#else /* not USE_BG_COLOR */
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_GRAPHICS)
#endif /* not USE_BG_COLOR */
void refresh(void)
{
    int line, col, pcol;
    int pline = CurLine;
    int moved = RF_NEED_TO_MOVE;
#ifdef USE_M17N
    char** pc;
#else
    char* pc;
#endif
    l_prop *pr, mode = 0;
    l_prop color = COL_FTERM;
#ifdef USE_BG_COLOR
    l_prop bcolor = COL_BTERM;
#endif /* USE_BG_COLOR */
    short* dirty;

#ifdef USE_M17N
    wc_putc_init(InnerCharset, DisplayCharset);
#endif
    for (line = 0; line <= LASTLINE(); line++) {
        dirty = &ScreenImage[line]->isdirty;
        if (*dirty & L_DIRTY) {
            *dirty &= ~L_DIRTY;
            pc = ScreenImage[line]->lineimage;
            pr = ScreenImage[line]->lineprop;
            for (col = 0; col < getRuntime()->cols && !(pr[col] & S_EOL); col++) {
                if (*dirty & L_NEED_CE && col >= ScreenImage[line]->eol) {
                    if (need_redraw(pc[col], pr[col], SPACE, 0))
                        break;
                } else {
                    if (pr[col] & S_DIRTY)
                        break;
                }
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                pcol = ScreenImage[line]->eol;
                if (pcol >= getRuntime()->cols) {
                    *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
                    pcol = col;
                }
            } else {
                pcol = col;
            }
            if (line < getRuntime()->lines - 2 && pline == line - 1 && pcol == 0) {
                switch (moved) {
                case RF_NEED_TO_MOVE:
                    tty_MOVE(line, 0);
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
                tty_MOVE(line, pcol);
                moved = RF_CR_OK;
            }
            if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
                writestr(getRuntime()->T_ce);
                if (col != pcol)
                    tty_MOVE(line, col);
            }
            pline = line;
            pcol = col;
            for (; col < getRuntime()->cols; col++) {
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
                if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) || (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) || (!(pr[col] & S_BOLD) && (mode & S_BOLD)) || (!(pr[col] & S_COLORED) && (mode & S_COLORED))
#ifdef USE_BG_COLOR
                    || (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED))
#endif /* USE_BG_COLOR */
                    || (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS))) {
                    if ((mode & S_COLORED)
#ifdef USE_BG_COLOR
                        || (mode & S_BCOLORED)
#endif /* USE_BG_COLOR */
                    )
                        writestr(getRuntime()->T_op);
                    if (mode & S_GRAPHICS)
                        writestr(getRuntime()->T_ae);
                    writestr(getRuntime()->T_me);
                    mode &= ~M_MEND;
                }
                if ((*dirty & L_NEED_CE && col >= ScreenImage[line]->eol) ? need_redraw(pc[col], pr[col], SPACE,
                                                                                0)
                                                                          : (pr[col] & S_DIRTY)) {
                    if (pcol == col - 1)
                        writestr(getRuntime()->T_nd);
                    else if (pcol != col)
                        tty_MOVE(line, col);

                    if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
                        writestr(getRuntime()->T_so);
                        mode |= S_STANDOUT;
                    }
                    if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
                        writestr(getRuntime()->T_us);
                        mode |= S_UNDERLINE;
                    }
                    if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
                        writestr(getRuntime()->T_md);
                        mode |= S_BOLD;
                    }
                    if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
                        color = (pr[col] & COL_FCOLOR);
                        mode = ((mode & ~COL_FCOLOR) | color);
                        writestr(color_seq(color));
                    }
#ifdef USE_BG_COLOR
                    if ((pr[col] & S_BCOLORED)
                        && (pr[col] ^ mode) & COL_BCOLOR) {
                        bcolor = (pr[col] & COL_BCOLOR);
                        mode = ((mode & ~COL_BCOLOR) | bcolor);
                        writestr(bcolor_seq(bcolor));
                    }
#endif /* USE_BG_COLOR */
                    if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
#ifdef USE_M17N
                        wc_putc_end(getRuntime()->tty_output_f);
#endif
                        if (!graph_enabled) {
                            graph_enabled = 1;
                            writestr(getRuntime()->T_eA);
                        }
                        writestr(getRuntime()->T_as);
                        mode |= S_GRAPHICS;
                    }
#ifdef USE_M17N
                    if (pr[col] & S_GRAPHICS)
                        write1(graphchar(*pc[col]));
                    else if (CHMODE(pr[col]) != C_WCHAR2)
                        wc_putc(pc[col], getRuntime()->tty_output_f);
#else
                    write1((pr[col] & S_GRAPHICS) ? graphchar(pc[col]) : pc[col]);
#endif
                    pcol = col + 1;
                }
            }
            if (col == getRuntime()->cols)
                moved = RF_NEED_TO_MOVE;
            for (; col < getRuntime()->cols && !(pr[col] & S_EOL); col++)
                pr[col] |= S_EOL;
        }
        *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
        if (mode & M_MEND) {
            if (mode & (S_COLORED
#ifdef USE_BG_COLOR
                    | S_BCOLORED
#endif /* USE_BG_COLOR */
                    ))
                writestr(getRuntime()->T_op);
            if (mode & S_GRAPHICS) {
                writestr(getRuntime()->T_ae);
#ifdef USE_M17N
                wc_putc_clear_status();
#endif
            }
            writestr(getRuntime()->T_me);
            mode &= ~M_MEND;
        }
    }
#ifdef USE_M17N
    wc_putc_end(getRuntime()->tty_output_f);
#endif
    tty_MOVE(CurLine, CurColumn);
    flush_tty();
}

void clear(void)
{
    int i, j;
    l_prop* p;
    writestr(getRuntime()->T_cl);
    move(0, 0);
    for (i = 0; i < getRuntime()->lines; i++) {
        ScreenImage[i]->isdirty = 0;
        p = ScreenImage[i]->lineprop;
        for (j = 0; j < getRuntime()->cols; j++) {
            p[j] = S_EOL;
        }
    }
    CurrentMode = C_ASCII;
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
    Screen* t;
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
    Screen* t;
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

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void)
{ /* Clear to the end of line */
    int i;
    l_prop* lprop = ScreenImage[CurLine]->lineprop;

    if (lprop[CurColumn] & S_EOL)
        return;

    if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || ScreenImage[CurLine]->eol > CurColumn)
        ScreenImage[CurLine]->eol = CurColumn;

    ScreenImage[CurLine]->isdirty |= L_CLRTOEOL;
    touch_line();
    for (i = CurColumn; i < getRuntime()->cols && !(lprop[i] & S_EOL); i++) {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(void)
{
    int i, cli, cco;
    l_prop pr;

    if (!(CurrentMode & S_BCOLORED)) {
        clrtoeol();
        return;
    }
    cli = CurLine;
    cco = CurColumn;
    pr = CurrentMode;
    CurrentMode = (CurrentMode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (i = CurColumn; i < getRuntime()->cols; i++)
        addch(' ');
    move(cli, cco);
    CurrentMode = pr;
}

void clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

static void
clrtobot_eol(void (*clrtoeol)())
{
    int l, c;

    l = CurLine;
    c = CurColumn;
    (*clrtoeol)();
    CurColumn = 0;
    CurLine++;
    for (; CurLine < getRuntime()->lines; CurLine++)
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

void addstr(char* s)
{
#ifdef USE_M17N
    int len;

    while (*s != '\0') {
        len = wtf_len((wc_uchar*)s);
        addmch(s, len);
        s += len;
    }
#else
    while (*s != '\0')
        addch(*(s++));
#endif
}

void addnstr(char* s, int n)
{
    int i;
#ifdef USE_M17N
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
#else
    for (i = 0; i < n && *s != '\0'; i++)
        addch(*(s++));
#endif
}

void addnstr_sup(char* s, int n)
{
    int i;
#ifdef USE_M17N
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
#else
    for (i = 0; i < n && *s != '\0'; i++)
        addch(*(s++));
#endif
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

void term_title(char* s)
{
    if (!fmInitialized())
        return;
    if (title_str != NULL) {
        fprintf(getRuntime()->tty_output_f, title_str, s);
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

void bell(void)
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
        while (IS_DIGIT(c))
            c = getch();
    }
}

int sleep_till_anykey(int sec, int purge)
{
    fd_set rfd;
    struct timeval tim;
    int er, c, ret;
    struct termios ioval;

    tcgetattr(getRuntime()->tty_input, &ioval);
    term_raw();

    tim.tv_sec = sec;
    tim.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(getRuntime()->tty_input, &rfd);

    ret = select(getRuntime()->tty_input + 1, &rfd, 0, 0, &tim);
    if (ret > 0 && purge) {
        c = getch();
        if (c == ESC_CODE)
            skip_escseq();
    }
    er = tcsetattr(getRuntime()->tty_input, TCSANOW, &ioval);
    if (er == -1) {
        printf("Error occurred: errno=%d\n", errno);
        reset_error_exit(SIGNAL_ARGLIST);
    }
    return ret;
}

#ifdef USE_IMAGE
void touch_cursor(void)
{
#ifdef USE_M17N
    int i;
#endif
    touch_line();
#ifdef USE_M17N
    for (i = CurColumn; i >= 0; i--) {
        touch_column(i);
        if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = CurColumn + 1; i < getRuntime()->cols; i++) {
        if (CHMODE(ScreenImage[CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        touch_column(i);
    }
#else
    touch_column(CurColumn);
#endif
}
#endif
