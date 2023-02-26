#include "term_screen.h"
#include "termcap_entry.h"
#include <string.h>
#include <alloc.h>
#include <wc.h>
#include <wtf.h>
#include <myctype.h>
#include <sstream>
#include <vector>

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

int LINES, COLS;
#if defined(__CYGWIN__)
int LASTLINE;
#endif /* defined(__CYGWIN__) */

#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)

#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#define M_MEND \
    (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS)

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

static int max_LINES = 0, max_COLS = 0;
static int tab_step = 8;
static int CurLine, CurColumn;
static Screen *ScreenElem = nullptr, **ScreenImage = nullptr;
static l_prop CurrentMode = 0;
static int graph_enabled = 0;

static char *color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + 30);
    return seqbuf;
}

static char *bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

void move(int line, int column)
{
    if (line >= 0 && line < LINES)
        CurLine = line;
    if (column >= 0 && column < COLS)
        CurColumn = column;
}

void clear(void)
{
    int i, j;
    l_prop *p;
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
    move(0, 0);
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

void setfcolor(int color)
{
    CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 8);
}

void setbcolor(int color)
{
    CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 12);
}

TermPos TermScreen::pos() const
{
    return {static_cast<uint16_t>(CurLine), static_cast<uint16_t>(CurColumn)};
}

std::vector<char> TermScreen::render(const TermcapEntry &g_entry)
{
    int line, col, pcol;
    int pline = CurLine;
    int moved = RF_NEED_TO_MOVE;
    char **pc;
    l_prop *pr, mode = 0;
    l_prop color = COL_FTERM;
    l_prop bcolor = COL_BTERM;
    short *dirty;

    std::vector<char> buffer;
    auto write = [&buffer](const char *p, size_t len) mutable
    {
        auto pos = buffer.size();
        buffer.resize(pos + len);
        std::copy(p, p + len, buffer.data() + pos);
    };
    auto writestr = [&write](const char *str)
    {
        write(str, strlen(str));
    };
    auto write1 = [&buffer](char c) mutable
    {
        buffer.push_back(c);
    };
    auto MOVE = [&writestr, &g_entry](int line, int column)
    {
        writestr(g_entry.move(line, column));
    };

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
                    write1('\n');
                    write1('\r');
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
                writestr(g_entry.T_ce);
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
                        writestr(g_entry.T_op);
                    if (mode & S_GRAPHICS)
                        writestr(g_entry.T_ae);
                    writestr(g_entry.T_me);
                    mode &= ~M_MEND;
                }
                if ((*dirty & L_NEED_CE && col >= ScreenImage[line]->eol)
                        ? need_redraw(pc[col], pr[col], SPACE, 0)
                        : (pr[col] & S_DIRTY))
                {
                    if (pcol == col - 1)
                        writestr(g_entry.T_nd);
                    else if (pcol != col)
                        MOVE(line, col);

                    if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT))
                    {
                        writestr(g_entry.T_so);
                        mode |= S_STANDOUT;
                    }
                    if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE))
                    {
                        writestr(g_entry.T_us);
                        mode |= S_UNDERLINE;
                    }
                    if ((pr[col] & S_BOLD) && !(mode & S_BOLD))
                    {
                        writestr(g_entry.T_md);
                        mode |= S_BOLD;
                    }
                    if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR)
                    {
                        color = (pr[col] & COL_FCOLOR);
                        mode = ((mode & ~COL_FCOLOR) | color);
                        writestr(color_seq(color));
                    }
                    if ((pr[col] & S_BCOLORED) && (pr[col] ^ mode) & COL_BCOLOR)
                    {
                        bcolor = (pr[col] & COL_BCOLOR);
                        mode = ((mode & ~COL_BCOLOR) | bcolor);
                        writestr(bcolor_seq(bcolor));
                    }
                    if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS))
                    {
                        wc_putc_end(write);
                        if (!graph_enabled)
                        {
                            graph_enabled = 1;
                            writestr(g_entry.T_eA);
                        }
                        writestr(g_entry.T_as);
                        mode |= S_GRAPHICS;
                    }
                    if (pr[col] & S_GRAPHICS)
                        write1(g_entry.graphchar(*pc[col]));
                    else if (CHMODE(pr[col]) != C_WCHAR2)
                        wc_putc(pc[col], write);
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
                writestr(g_entry.T_op);
            if (mode & S_GRAPHICS)
            {
                writestr(g_entry.T_ae);
                wc_putc_clear_status();
            }
            writestr(g_entry.T_me);
            mode &= ~M_MEND;
        }
    }
    wc_putc_end(write);
    buffer.push_back(0);
    return buffer;
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
        // scroll_raw();
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
    // if (g_entry.T_sr && *g_entry.T_sr)
    // {
    //     MOVE(0, 0);
    //     for (i = 0; i < n; i++)
    //     {
    //         t = ScreenImage[i];
    //         t->isdirty = 0;
    //         for (j = 0; j < COLS; j++)
    //             t->lineprop[j] = S_EOL;
    //         tty::writestr(g_entry.T_sr);
    //     }
    //     move(cli, cco);
    // }
    // else
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
