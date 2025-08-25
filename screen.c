#include "screen.h"
#include "term_size.h"
#include "term_entry.h"
#include "graphicchar.h"
#include "fm.h"
#include <stdio.h>
#include <string.h>

int Do_not_use_ti_te = 0;

struct VirtualTerm g_screen;
struct VirtualTerm* getScreen()
{
    return &g_screen;
}

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

#define CHMODE(c) ((c) & C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define SETCH(var, ch, len) ((var) = New_Reuse(char, (var), (len) + 1), \
    strncpy((var), (ch), (len + 1)))

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

#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))

static bool ISDIRTY(enum LineStatus d) { return d & L_DIRTY; }
static bool ISUNUSED(enum LineStatus d) { return d & L_UNUSED; }
static bool NEED_CE(enum LineStatus d) { return d & L_NEED_CE; }

void setupscreen(struct VirtualTerm* vt)
{
    if (getLines() + 1 > vt->max_LINES) {
        vt->max_LINES = getLines() + 1;
        vt->max_COLS = 0;
        vt->ScreenElem = New_N(Screen, vt->max_LINES);
        vt->ScreenImage = New_N(Screen*, vt->max_LINES);
    }
    if (getCols() + 1 > vt->max_COLS) {
        vt->max_COLS = getCols() + 1;
        for (int i = 0; i < vt->max_LINES; i++) {
            vt->ScreenElem[i].lineimage = New_N(char*, vt->max_COLS);
            bzero((void*)vt->ScreenElem[i].lineimage, vt->max_COLS * sizeof(char*));
            vt->ScreenElem[i].lineprop = New_N(l_prop, vt->max_COLS);
        }
    }
    {
        int i = 0;
        for (; i < getLines(); i++) {
            vt->ScreenImage[i] = &vt->ScreenElem[i];
            vt->ScreenImage[i]->lineprop[0] = S_EOL;
            vt->ScreenImage[i]->isdirty = 0;
        }
        for (; i < vt->max_LINES; i++) {
            vt->ScreenElem[i].isdirty = L_UNUSED;
        }
    }
}

void move(struct VirtualTerm* vt, int line, int column)
{
    if (line >= 0 && line < getLines())
        vt->CurLine = line;
    if (column >= 0 && column < getCols())
        vt->CurColumn = column;
}

#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)

static int
need_redraw(char* c1, l_prop pr1, char* c2, l_prop pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void touch_column(struct VirtualTerm* vt, int col)
{
    if (col >= 0 && col < getCols())
        vt->ScreenImage[vt->CurLine]->lineprop[col] |= S_DIRTY;
}

#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

#define SPACE " "

void addch(struct VirtualTerm* vt, char c)
{
    addmch(vt, &c, 1);
}

void addmch(struct VirtualTerm* vt, char* pc, size_t len)
{
    l_prop* pr;
    int dest, i;
    static Str tmp = NULL;
    char** p;
    char c = *pc;
    int width = wtf_width((wc_uchar*)pc);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;

    if (vt->CurColumn == getCols())
        wrap(vt);
    if (vt->CurColumn >= getCols())
        return;
    p = vt->ScreenImage[vt->CurLine]->lineimage;
    pr = vt->ScreenImage[vt->CurLine]->lineprop;

    if (pr[vt->CurColumn] & S_EOL) {
        if (c == ' ' && !(vt->CurrentMode & M_SPACE)) {
            vt->CurColumn++;
            return;
        }
        for (i = vt->CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
        }
    }

    if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
        SETCHMODE(vt->CurrentMode, C_CTRL);
    else if (len > 1)
        SETCHMODE(vt->CurrentMode, C_WCHAR1);
    else if (!IS_CNTRL(c))
        SETCHMODE(vt->CurrentMode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = vt->CurColumn + width - 1;
    if (i < getCols() && (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], pc, vt->CurrentMode)) || ((pr[i] & S_UNDERLINE) && !(vt->CurrentMode & S_UNDERLINE)))) {
        touch_line(vt);
        i++;
        if (i < getCols()) {
            touch_column(vt, i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < getCols() && CHMODE(pr[i]) == C_WCHAR2; i++)
                    touch_column(vt, i);
            }
        }
    }

    if (vt->CurColumn + width > getCols()) {
        touch_line(vt);
        for (i = vt->CurColumn; i < getCols(); i++) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(vt, i);
        }
        wrap(vt);
        if (vt->CurColumn + width > getCols())
            return;
        p = vt->ScreenImage[vt->CurLine]->lineimage;
        pr = vt->ScreenImage[vt->CurLine]->lineprop;
    }
    if (CHMODE(pr[vt->CurColumn]) == C_WCHAR2) {
        touch_line(vt);
        for (i = vt->CurColumn - 1; i >= 0; i--) {
            l_prop l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(vt, i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(vt->CurrentMode) != C_CTRL) {
        if (need_redraw(p[vt->CurColumn], pr[vt->CurColumn], pc, vt->CurrentMode)) {
            SETCH(p[vt->CurColumn], pc, len);
            SETPROP(pr[vt->CurColumn], vt->CurrentMode);
            touch_line(vt);
            touch_column(vt, vt->CurColumn);
            SETCHMODE(vt->CurrentMode, C_WCHAR2);
            for (i = vt->CurColumn + 1; i < vt->CurColumn + width; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[vt->CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                touch_column(vt, i);
            }
            for (; i < getCols() && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                touch_column(vt, i);
            }
        }
        vt->CurColumn += width;
    } else if (c == '\t') {
        dest = (vt->CurColumn + vt->tab_step) / vt->tab_step * vt->tab_step;
        if (dest >= getCols()) {
            wrap(vt);
            touch_line(vt);
            dest = vt->tab_step;
            p = vt->ScreenImage[vt->CurLine]->lineimage;
            pr = vt->ScreenImage[vt->CurLine]->lineprop;
        }
        for (i = vt->CurColumn; i < dest; i++) {
            if (need_redraw(p[i], pr[i], SPACE, vt->CurrentMode)) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], vt->CurrentMode);
                touch_line(vt);
                touch_column(vt, i);
            }
        }
        vt->CurColumn = i;
    } else if (c == '\n') {
        wrap(vt);
    } else if (c == '\r') { /* Carriage return */
        vt->CurColumn = 0;
    } else if (c == '\b' && vt->CurColumn > 0) { /* Backspace */
        vt->CurColumn--;
        while (vt->CurColumn > 0 && CHMODE(pr[vt->CurColumn]) == C_WCHAR2)
            vt->CurColumn--;
    }
}

void wrap(struct VirtualTerm* vt)
{
    if (vt->CurLine == getLines() - 1)
        return;
    vt->CurLine++;
    vt->CurColumn = 0;
}

void touch_line(struct VirtualTerm* vt)
{
    if (!(vt->ScreenImage[vt->CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < getCols(); i++)
            vt->ScreenImage[vt->CurLine]->lineprop[i] &= ~S_DIRTY;
        vt->ScreenImage[vt->CurLine]->isdirty |= L_DIRTY;
    }
}

void standout(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_STANDOUT;
}

void standend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_STANDOUT;
}

void toggle_stand(struct VirtualTerm* vt)
{
    int i;
    l_prop* pr = vt->ScreenImage[vt->CurLine]->lineprop;
    pr[vt->CurColumn] ^= S_STANDOUT;
    if (CHMODE(pr[vt->CurColumn]) != C_WCHAR2) {
        for (i = vt->CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void bold(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_BOLD;
}

void boldend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_BOLD;
}

void underline(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_UNDERLINE;
}

void underlineend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_UNDERLINE;
}

void graphstart(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_GRAPHICS;
}

void graphend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_GRAPHICS;
}

void setfcolor(struct VirtualTerm* vt, int color)
{
    vt->CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        vt->CurrentMode |= (((color & 7) | 8) << 8);
}

static char*
color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    return seqbuf;
}

void setbcolor(struct VirtualTerm* vt, int color)
{
    vt->CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        vt->CurrentMode |= (((color & 7) | 8) << 12);
}

static char*
bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

static void MOVE(const struct Writer* writer, int line, int column)
{
    putsWriter(writer, getMoveXY(column, line));
}

enum RF_MODE {
    RF_NEED_TO_MOVE = 0,
    RF_CR_OK = 1,
    RF_NONEED_TO_MOVE = 2,
};
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS)
void refreshLine(const struct Writer* writer, struct VirtualTerm* vt, int line)
{
    struct TermEntry* t = getTermEntry();
    int pline = vt->CurLine;
    enum RF_MODE moved = RF_NEED_TO_MOVE;
    l_prop mode = 0;
    l_prop color = COL_FTERM;
    l_prop bcolor = COL_BTERM;
    Screen* l = vt->ScreenImage[line];
    enum LineStatus* dirty = &l->isdirty;
    if (*dirty & L_DIRTY) {
        *dirty &= ~L_DIRTY;
        char** pc = l->lineimage;
        l_prop* pr = l->lineprop;
        int col = 0;
        for (; col < getCols() && !(pr[col] & S_EOL); col++) {
            if (*dirty & L_NEED_CE && col >= l->eol) {
                if (need_redraw(pc[col], pr[col], SPACE, 0))
                    break;
            } else {
                if (pr[col] & S_DIRTY)
                    break;
            }
        }

        int pcol;
        if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
            pcol = l->eol;
            if (pcol >= getCols()) {
                *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
                pcol = col;
            }
        } else {
            pcol = col;
        }
        if (line < getLines() - 2 && pline == line - 1 && pcol == 0) {
            switch (moved) {
            case RF_NEED_TO_MOVE:
                MOVE(writer, line, 0);
                moved = RF_CR_OK;
                break;
            case RF_CR_OK:
                putWriter(writer, '\n');
                putWriter(writer, '\r');
                break;
            case RF_NONEED_TO_MOVE:
                moved = RF_CR_OK;
                break;
            }
        } else {
            MOVE(writer, line, pcol);
            moved = RF_CR_OK;
        }
        if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
            putsWriter(writer, t->ce);
            if (col != pcol)
                MOVE(writer, line, col);
        }
        pline = line;
        pcol = col;
        for (; col < getCols(); col++) {
            if (pr[col] & S_EOL)
                break;

            /*
             * some terminal emulators do linefeed when a
             * character is put on getCols()-th column. this behavior
             * is different from one of vt100, but such terminal
             * emulators are used as vt100-compatible
             * emulators. This behaviour causes scroll when a
             * character is drawn on (getCols()-1,getLines()-1) point.  To
             * avoid the scroll, I prohibit to draw character on
             * (getCols()-1,getLines()-1).
             */
            if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) || (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) || (!(pr[col] & S_BOLD) && (mode & S_BOLD)) || (!(pr[col] & S_COLORED) && (mode & S_COLORED))
                || (!(pr[col] & S_BCOLORED) && (mode & S_BCOLORED))
                || (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS))) {
                if ((mode & S_COLORED)
                    || (mode & S_BCOLORED))
                    putsWriter(writer, t->op);
                if (mode & S_GRAPHICS)
                    putsWriter(writer, t->ae);
                putsWriter(writer, t->me);
                mode &= ~M_MEND;
            }
            if ((*dirty & L_NEED_CE && col >= l->eol) ? need_redraw(pc[col], pr[col], SPACE,
                                                            0)
                                                      : (pr[col] & S_DIRTY)) {
                if (pcol == col - 1)
                    putsWriter(writer, t->nd);
                else if (pcol != col)
                    MOVE(writer, line, col);

                if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
                    putsWriter(writer, t->so);
                    mode |= S_STANDOUT;
                }
                if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
                    putsWriter(writer, t->us);
                    mode |= S_UNDERLINE;
                }
                if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
                    putsWriter(writer, t->md);
                    mode |= S_BOLD;
                }
                if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
                    color = (pr[col] & COL_FCOLOR);
                    mode = ((mode & ~COL_FCOLOR) | color);
                    putsWriter(writer, color_seq(color));
                }
                if ((pr[col] & S_BCOLORED)
                    && (pr[col] ^ mode) & COL_BCOLOR) {
                    bcolor = (pr[col] & COL_BCOLOR);
                    mode = ((mode & ~COL_BCOLOR) | bcolor);
                    putsWriter(writer, bcolor_seq(bcolor));
                }
                if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
                    wc_putc_end(writer);
                    if (!vt->graph_enabled) {
                        vt->graph_enabled = 1;
                        putsWriter(writer, t->eA);
                    }
                    putsWriter(writer, t->as);
                    mode |= S_GRAPHICS;
                }
                if (pr[col] & S_GRAPHICS)
                    putWriter(writer, graphchar(*pc[col]));
                else if (CHMODE(pr[col]) != C_WCHAR2) {
                    wc_putc(writer, pc[col]);
                }
                pcol = col + 1;
            }
        }
        if (col == getCols())
            moved = RF_NEED_TO_MOVE;
        for (; col < getCols() && !(pr[col] & S_EOL); col++)
            pr[col] |= S_EOL;
    }
    *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
    if (mode & M_MEND) {
        if (mode & (S_COLORED | S_BCOLORED))
            putsWriter(writer, t->op);
        if (mode & S_GRAPHICS) {
            putsWriter(writer, t->ae);
            wc_putc_clear_status();
        }
        putsWriter(writer, t->me);
        mode &= ~M_MEND;
    }
}

// Screen to STDOUT
void refresh(const struct Writer* writer)
{
    struct VirtualTerm* vt = getScreen();
    wc_putc_init(InnerCharset, DisplayCharset);
    for (int line = 0; line <= getLines() - 1; line++) {
        refreshLine(writer, vt, line);
    }
    wc_putc_end(writer);
    MOVE(writer, vt->CurLine, vt->CurColumn);
    flushWriter(writer);
}

void clear(const struct Writer* writer)
{
    struct VirtualTerm* vt = getScreen();
    struct TermEntry* t = getTermEntry();
    int i, j;
    l_prop* p;
    putsWriter(writer, t->cl);
    move(vt, 0, 0);
    for (i = 0; i < getLines(); i++) {
        vt->ScreenImage[i]->isdirty = 0;
        p = vt->ScreenImage[i]->lineprop;
        for (j = 0; j < getCols(); j++) {
            p[j] = S_EOL;
        }
    }
    vt->CurrentMode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(struct VirtualTerm* vt)
{ /* Clear to the end of line */
    int i;
    l_prop* lprop = vt->ScreenImage[vt->CurLine]->lineprop;

    if (lprop[vt->CurColumn] & S_EOL)
        return;

    if (!(vt->ScreenImage[vt->CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || vt->ScreenImage[vt->CurLine]->eol > vt->CurColumn)
        vt->ScreenImage[vt->CurLine]->eol = vt->CurColumn;

    vt->ScreenImage[vt->CurLine]->isdirty |= L_CLRTOEOL;
    touch_line(vt);
    for (i = vt->CurColumn; i < getCols() && !(lprop[i] & S_EOL); i++) {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(struct VirtualTerm* vt)
{
    if (!(vt->CurrentMode & S_BCOLORED)) {
        clrtoeol(vt);
        return;
    }
    int cli = vt->CurLine;
    int cco = vt->CurColumn;
    l_prop pr = vt->CurrentMode;
    vt->CurrentMode = (vt->CurrentMode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (int i = vt->CurColumn; i < getCols(); i++)
        addch(vt, ' ');
    move(vt, cli, cco);
    vt->CurrentMode = pr;
}

void clrtoeolx(struct VirtualTerm* vt)
{
    clrtoeol_with_bcolor(vt);
}

typedef void (*ClearFunc)(struct VirtualTerm* vt);

static void
clrtobot_eol(struct VirtualTerm* vt, ClearFunc clrtoeol)
{
    int l = vt->CurLine;
    int c = vt->CurColumn;
    (*clrtoeol)(vt);
    vt->CurColumn = 0;
    vt->CurLine++;
    for (; vt->CurLine < getLines(); vt->CurLine++)
        (*clrtoeol)(vt);
    vt->CurLine = l;
    vt->CurColumn = c;
}

void clrtobot(struct VirtualTerm* vt)
{
    clrtobot_eol(vt, clrtoeol);
}

void clrtobotx(struct VirtualTerm* vt)
{
    clrtobot_eol(vt, clrtoeolx);
}

void addstr(struct VirtualTerm* vt, char* s)
{
    while (*s != '\0') {
        int len = wtf_len((wc_uchar*)s);
        addmch(vt, s, len);
        s += len;
    }
}

void addnstr(struct VirtualTerm* vt, char* s, int n)
{
    for (int i = 0; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        addmch(vt, s, len);
        s += len;
        i += width;
    }
}

void addnstr_sup(struct VirtualTerm* vt, char* s, int n)
{
    int i = 0;
    for (; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        addmch(vt, s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        addch(vt, ' ');
}

void bell(const struct Writer* writer)
{
    putWriter(writer, 7);
}

void touch_cursor(struct VirtualTerm* vt)
{
    int i;
    touch_line(vt);
    for (i = vt->CurColumn; i >= 0; i--) {
        touch_column(vt, i);
        if (CHMODE(vt->ScreenImage[vt->CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = vt->CurColumn + 1; i < getCols(); i++) {
        if (CHMODE(vt->ScreenImage[vt->CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        touch_column(vt, i);
    }
}
