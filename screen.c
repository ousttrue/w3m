#include "screen.h"
#include <gcstr/gcstr.h>
#include <stdlib.h>
#include <wc.h>
#include <wtf.h>
#include <string.h>

#define SPACE " "

static int tab_step = 8;
static int max_LINES = 0;
static int max_COLS = 0;
static struct ScreenLine* ScreenElem = 0;

static struct Screen g_screen = {
    .lines = 0,
    .cols = 0,
    .ScreenImage = 0,
    .CurLine = 0,
    .CurColumn = 0,
    .mode = 0,
};

struct Screen getScreen()
{
    return g_screen;
}

void set_screen_mode(uint16_t mode)
{
    g_screen.mode = mode;
}

void setupscreen(int lines, int cols)
{
    int i;

    if (lines + 1 > max_LINES) {
        max_LINES = lines + 1;
        max_COLS = 0;
        ScreenElem = New_N(struct ScreenLine, max_LINES);
        g_screen.ScreenImage = New_N(struct ScreenLine*, max_LINES);
    }
    g_screen.lines = lines;

    if (cols + 1 > max_COLS) {
        max_COLS = cols + 1;
        for (i = 0; i < max_LINES; i++) {
            ScreenElem[i].lineimage = New_N(char*, max_COLS);
            memset(ScreenElem[i].lineimage, 0, max_COLS * sizeof(char*));
            ScreenElem[i].lineprop = New_N(uint16_t, max_COLS);
        }
    }
    g_screen.cols = cols;

    for (i = 0; i < g_screen.lines; i++) {
        g_screen.ScreenImage[i] = &ScreenElem[i];
        g_screen.ScreenImage[i]->lineprop[0] = S_EOL;
        g_screen.ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++) {
        ScreenElem[i].isdirty = L_UNUSED;
    }

    clear();
}

void clear(void)
{
    // writestr(T_cl);
    // move(0, 0);
    int i, j;
    uint16_t* p;
    struct Screen sc = getScreen();
    for (i = 0; i < g_screen.lines; i++) {
        sc.ScreenImage[i]->isdirty = 0;
        p = sc.ScreenImage[i]->lineprop;
        for (j = 0; j < g_screen.cols; j++) {
            p[j] = S_EOL;
        }
    }
    set_screen_mode(C_ASCII);
}

void move(int line, int column)
{
    if (line >= 0 && line < g_screen.lines)
        g_screen.CurLine = line;
    if (column >= 0 && column < g_screen.cols)
        g_screen.CurColumn = column;
}

void wrap(void)
{
    if (g_screen.CurLine == g_screen.lines - 1)
        return;
    g_screen.CurLine++;
    g_screen.CurColumn = 0;
}

void touch_cursor(void)
{
    int i;
    touch_line();
    for (i = g_screen.CurColumn; i >= 0; i--) {
        touch_column(i);
        if (CHMODE(g_screen.ScreenImage[g_screen.CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = g_screen.CurColumn + 1; i < g_screen.cols; i++) {
        if (CHMODE(g_screen.ScreenImage[g_screen.CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        touch_column(i);
    }
}

void touch_column(int col)
{
    if (col >= 0 && col < g_screen.cols)
        g_screen.ScreenImage[g_screen.CurLine]->lineprop[col] |= S_DIRTY;
}

void touch_line(void)
{
    if (!(g_screen.ScreenImage[g_screen.CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < g_screen.cols; i++)
            g_screen.ScreenImage[g_screen.CurLine]->lineprop[i] &= ~S_DIRTY;
        g_screen.ScreenImage[g_screen.CurLine]->isdirty |= L_DIRTY;
    }
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void)
{ /* Clear to the end of line */
    int i;
    uint16_t* lprop = g_screen.ScreenImage[g_screen.CurLine]->lineprop;

    if (lprop[g_screen.CurColumn] & S_EOL)
        return;

    if (!(g_screen.ScreenImage[g_screen.CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || g_screen.ScreenImage[g_screen.CurLine]->eol > g_screen.CurColumn)
        g_screen.ScreenImage[g_screen.CurLine]->eol = g_screen.CurColumn;

    g_screen.ScreenImage[g_screen.CurLine]->isdirty |= L_CLRTOEOL;
    touch_line();
    for (i = g_screen.CurColumn; i < g_screen.cols && !(lprop[i] & S_EOL); i++) {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(void)
{
    int i, cli, cco;
    uint16_t pr;

    if (!(g_screen.mode & S_BCOLORED)) {
        clrtoeol();
        return;
    }
    cli = g_screen.CurLine;
    cco = g_screen.CurColumn;
    pr = g_screen.mode;
    g_screen.mode = (g_screen.mode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (i = g_screen.CurColumn; i < g_screen.cols; i++)
        addch(' ');
    move(cli, cco);
    g_screen.mode = pr;
}

void clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

static void
clrtobot_eol(void (*clrtoeol)())
{
    int l, c;

    l = g_screen.CurLine;
    c = g_screen.CurColumn;
    (*clrtoeol)();
    g_screen.CurColumn = 0;
    g_screen.CurLine++;
    for (; g_screen.CurLine < g_screen.lines; g_screen.CurLine++)
        (*clrtoeol)();
    g_screen.CurLine = l;
    g_screen.CurColumn = c;
}

void clrtobot(void)
{
    clrtobot_eol(clrtoeol);
}

void clrtobotx(void)
{
    clrtobot_eol(clrtoeolx);
}

bool is_need_redraw(const char* c1, uint16_t pr1, const char* c2, uint16_t pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void addmch(const char* pc, size_t len)
{
    uint16_t* pr;
    int dest, i;
    static Str tmp = NULL;
    char** p;
    char c = *pc;
    int width = wtf_width((wc_uchar*)pc);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, pc, len);
    pc = tmp->ptr;

    if (g_screen.CurColumn == g_screen.cols)
        wrap();
    if (g_screen.CurColumn >= g_screen.cols)
        return;
    p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
    pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;

    if (pr[g_screen.CurColumn] & S_EOL) {
        if (c == ' ' && !(g_screen.mode & M_SPACE)) {
            g_screen.CurColumn++;
            return;
        }
        for (i = g_screen.CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
        }
    }

    if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
        SETCHMODE(g_screen.mode, C_CTRL);
    else if (len > 1)
        SETCHMODE(g_screen.mode, C_WCHAR1);
    else if (!IS_CNTRL(c))
        SETCHMODE(g_screen.mode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = g_screen.CurColumn + width - 1;
    if (i < g_screen.cols && (((pr[i] & S_BOLD) && is_need_redraw(p[i], pr[i], pc, g_screen.mode)) || ((pr[i] & S_UNDERLINE) && !(g_screen.mode & S_UNDERLINE)))) {
        touch_line();
        i++;
        if (i < g_screen.cols) {
            touch_column(i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++)
                    touch_column(i);
            }
        }
    }

    if (g_screen.CurColumn + width > g_screen.cols) {
        touch_line();
        for (i = g_screen.CurColumn; i < g_screen.cols; i++) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
        }
        wrap();
        if (g_screen.CurColumn + width > g_screen.cols)
            return;
        p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
        pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
    }
    if (CHMODE(pr[g_screen.CurColumn]) == C_WCHAR2) {
        touch_line();
        for (i = g_screen.CurColumn - 1; i >= 0; i--) {
            uint16_t l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(g_screen.mode) != C_CTRL) {
        if (is_need_redraw(p[g_screen.CurColumn], pr[g_screen.CurColumn], pc, g_screen.mode)) {
            SETCH(p[g_screen.CurColumn], pc, len);
            SETPROP(pr[g_screen.CurColumn], g_screen.mode);
            touch_line();
            touch_column(g_screen.CurColumn);
            SETCHMODE(g_screen.mode, C_WCHAR2);
            for (i = g_screen.CurColumn + 1; i < g_screen.CurColumn + width; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[g_screen.CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                touch_column(i);
            }
            for (; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                touch_column(i);
            }
        }
        g_screen.CurColumn += width;
    } else if (c == '\t') {
        dest = (g_screen.CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= g_screen.cols) {
            wrap();
            touch_line();
            dest = tab_step;
            p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
            pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
        }
        for (i = g_screen.CurColumn; i < dest; i++) {
            if (is_need_redraw(p[i], pr[i], SPACE, g_screen.mode)) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], g_screen.mode);
                touch_line();
                touch_column(i);
            }
        }
        g_screen.CurColumn = i;
    } else if (c == '\n') {
        wrap();
    } else if (c == '\r') { /* Carriage return */
        g_screen.CurColumn = 0;
    } else if (c == '\b' && g_screen.CurColumn > 0) { /* Backspace */
        g_screen.CurColumn--;
        while (g_screen.CurColumn > 0 && CHMODE(pr[g_screen.CurColumn]) == C_WCHAR2)
            g_screen.CurColumn--;
    }
}

void standout(void)
{
    g_screen.mode |= S_STANDOUT;
}

void standend(void)
{
    g_screen.mode &= ~S_STANDOUT;
}

void toggle_stand(void)
{
    int i;
    uint16_t* pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
    pr[g_screen.CurColumn] ^= S_STANDOUT;
    if (CHMODE(pr[g_screen.CurColumn]) != C_WCHAR2) {
        for (i = g_screen.CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void bold(void)
{
    g_screen.mode |= S_BOLD;
}

void boldend(void)
{
    g_screen.mode &= ~S_BOLD;
}

void underline(void)
{
    g_screen.mode |= S_UNDERLINE;
}

void underlineend(void)
{
    g_screen.mode &= ~S_UNDERLINE;
}

void graphstart(void)
{
    g_screen.mode |= S_GRAPHICS;
}

void graphend(void)
{
    g_screen.mode &= ~S_GRAPHICS;
}

void setfcolor(int color)
{
    g_screen.mode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 8);
}

void setbcolor(int color)
{
    g_screen.mode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 12);
}
