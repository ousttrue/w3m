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

struct Screen scr_get()
{
    return g_screen;
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

    scr_clear();
}

void scr_clear(void)
{
    // writestr(T_cl);
    scr_move(0, 0);
    struct Screen sc = scr_get();
    for (int i = 0; i < g_screen.lines; i++) {
        sc.ScreenImage[i]->isdirty = 0;
        uint16_t* p = sc.ScreenImage[i]->lineprop;
        for (int j = 0; j < g_screen.cols; j++) {
            p[j] = S_EOL;
        }
    }
    g_screen.mode = C_ASCII;
}

void scr_move(int line, int column)
{
    if (line >= 0 && line < g_screen.lines)
        g_screen.CurLine = line;
    if (column >= 0 && column < g_screen.cols)
        g_screen.CurColumn = column;
}

void scr_wrap(void)
{
    if (g_screen.CurLine == g_screen.lines - 1)
        return;
    g_screen.CurLine++;
    g_screen.CurColumn = 0;
}

void scr_touch_cursor(void)
{
    int i;
    scr_touch_line();
    for (i = g_screen.CurColumn; i >= 0; i--) {
        scr_touch_column(i);
        if (CHMODE(g_screen.ScreenImage[g_screen.CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = g_screen.CurColumn + 1; i < g_screen.cols; i++) {
        if (CHMODE(g_screen.ScreenImage[g_screen.CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        scr_touch_column(i);
    }
}

void scr_touch_column(int col)
{
    if (col >= 0 && col < g_screen.cols)
        g_screen.ScreenImage[g_screen.CurLine]->lineprop[col] |= S_DIRTY;
}

void scr_touch_line(void)
{
    if (!(g_screen.ScreenImage[g_screen.CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < g_screen.cols; i++)
            g_screen.ScreenImage[g_screen.CurLine]->lineprop[i] &= ~S_DIRTY;
        g_screen.ScreenImage[g_screen.CurLine]->isdirty |= L_DIRTY;
    }
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void scr_clrtoeol(void)
{ /* Clear to the end of line */
    int i;
    uint16_t* lprop = g_screen.ScreenImage[g_screen.CurLine]->lineprop;

    if (lprop[g_screen.CurColumn] & S_EOL)
        return;

    if (!(g_screen.ScreenImage[g_screen.CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || g_screen.ScreenImage[g_screen.CurLine]->eol > g_screen.CurColumn)
        g_screen.ScreenImage[g_screen.CurLine]->eol = g_screen.CurColumn;

    g_screen.ScreenImage[g_screen.CurLine]->isdirty |= L_CLRTOEOL;
    scr_touch_line();
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
        scr_clrtoeol();
        return;
    }
    cli = g_screen.CurLine;
    cco = g_screen.CurColumn;
    pr = g_screen.mode;
    g_screen.mode = (g_screen.mode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (i = g_screen.CurColumn; i < g_screen.cols; i++)
        scr_addch(' ');
    scr_move(cli, cco);
    g_screen.mode = pr;
}

void scr_clrtoeolx(void)
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

void scr_clrtobot(void)
{
    clrtobot_eol(scr_clrtoeol);
}

void scr_clrtobotx(void)
{
    clrtobot_eol(scr_clrtoeolx);
}

bool scr_is_need_redraw(const char* c1, uint16_t pr1, const char* c2, uint16_t pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void scr_addmch(const char* pc, size_t len)
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
        scr_wrap();
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
    if (i < g_screen.cols && (((pr[i] & S_BOLD) && scr_is_need_redraw(p[i], pr[i], pc, g_screen.mode)) || ((pr[i] & S_UNDERLINE) && !(g_screen.mode & S_UNDERLINE)))) {
        scr_touch_line();
        i++;
        if (i < g_screen.cols) {
            scr_touch_column(i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++)
                    scr_touch_column(i);
            }
        }
    }

    if (g_screen.CurColumn + width > g_screen.cols) {
        scr_touch_line();
        for (i = g_screen.CurColumn; i < g_screen.cols; i++) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            scr_touch_column(i);
        }
        scr_wrap();
        if (g_screen.CurColumn + width > g_screen.cols)
            return;
        p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
        pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
    }
    if (CHMODE(pr[g_screen.CurColumn]) == C_WCHAR2) {
        scr_touch_line();
        for (i = g_screen.CurColumn - 1; i >= 0; i--) {
            uint16_t l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            scr_touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(g_screen.mode) != C_CTRL) {
        if (scr_is_need_redraw(p[g_screen.CurColumn], pr[g_screen.CurColumn], pc, g_screen.mode)) {
            SETCH(p[g_screen.CurColumn], pc, len);
            SETPROP(pr[g_screen.CurColumn], g_screen.mode);
            scr_touch_line();
            scr_touch_column(g_screen.CurColumn);
            SETCHMODE(g_screen.mode, C_WCHAR2);
            for (i = g_screen.CurColumn + 1; i < g_screen.CurColumn + width; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[g_screen.CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                scr_touch_column(i);
            }
            for (; i < g_screen.cols && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                scr_touch_column(i);
            }
        }
        g_screen.CurColumn += width;
    } else if (c == '\t') {
        dest = (g_screen.CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= g_screen.cols) {
            scr_wrap();
            scr_touch_line();
            dest = tab_step;
            p = g_screen.ScreenImage[g_screen.CurLine]->lineimage;
            pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
        }
        for (i = g_screen.CurColumn; i < dest; i++) {
            if (scr_is_need_redraw(p[i], pr[i], SPACE, g_screen.mode)) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], g_screen.mode);
                scr_touch_line();
                scr_touch_column(i);
            }
        }
        g_screen.CurColumn = i;
    } else if (c == '\n') {
        scr_wrap();
    } else if (c == '\r') { /* Carriage return */
        g_screen.CurColumn = 0;
    } else if (c == '\b' && g_screen.CurColumn > 0) { /* Backspace */
        g_screen.CurColumn--;
        while (g_screen.CurColumn > 0 && CHMODE(pr[g_screen.CurColumn]) == C_WCHAR2)
            g_screen.CurColumn--;
    }
}

void scr_addstr(const char* s)
{
    while (*s != '\0') {
        int len = wtf_len((wc_uchar*)s);
        scr_addmch(s, len);
        s += len;
    }
}

void scr_addnstr(const char* s, int n)
{
    int i;
    for (i = 0; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        scr_addmch(s, len);
        s += len;
        i += width;
    }
}

/// space padding
void scr_addnstr_sup(const char* s, int n)
{
    int i;
    for (i = 0; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        scr_addmch(s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        scr_addch(' ');
}

void scr_standout(void)
{
    g_screen.mode |= S_STANDOUT;
}

void scr_standend(void)
{
    g_screen.mode &= ~S_STANDOUT;
}

void scr_toggle_stand(void)
{
    int i;
    uint16_t* pr = g_screen.ScreenImage[g_screen.CurLine]->lineprop;
    pr[g_screen.CurColumn] ^= S_STANDOUT;
    if (CHMODE(pr[g_screen.CurColumn]) != C_WCHAR2) {
        for (i = g_screen.CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void scr_bold(void)
{
    g_screen.mode |= S_BOLD;
}

void scr_boldend(void)
{
    g_screen.mode &= ~S_BOLD;
}

void scr_underline(void)
{
    g_screen.mode |= S_UNDERLINE;
}

void scr_underlineend(void)
{
    g_screen.mode &= ~S_UNDERLINE;
}

void scr_graphstart(void)
{
    g_screen.mode |= S_GRAPHICS;
}

void scr_graphend(void)
{
    g_screen.mode &= ~S_GRAPHICS;
}

void scr_setfcolor(int color)
{
    g_screen.mode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 8);
}

void scr_setbcolor(int color)
{
    g_screen.mode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        g_screen.mode |= (((color & 7) | 8) << 12);
}
