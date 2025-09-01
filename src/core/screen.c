#include "screen.h"
#include "screen_effects.h"
#include "Str.h"
#include "alloc.h"
#include "myctype.h"
#include "symbol.h"
#include "ui.h"
#include "ctrlcode.h"
#include <wc.h>
#include <wtf.h>
#include <stdio.h>
#include <string.h>

int Do_not_use_ti_te = 0;

struct VirtualTerm g_screen;
struct VirtualTerm* getScreen()
{
    return &g_screen;
}

#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))

static bool ISDIRTY(enum LineStatus d) { return d & L_DIRTY; }
static bool ISUNUSED(enum LineStatus d) { return d & L_UNUSED; }
static bool NEED_CE(enum LineStatus d) { return d & L_NEED_CE; }

void vt_setupscreen(struct VirtualTerm* vt, int rows, int cols)
{
    if (rows > vt->ROWS) {
        vt->COLS = 0;
        vt->ScreenElem = New_N(Screen, rows);
        vt->ScreenImage = New_N(Screen*, rows);
    }
    vt->ROWS = rows;

    if (cols > vt->COLS) {
        for (int i = 0; i < vt->ROWS; i++) {
            vt->ScreenElem[i].lineimage = New_N(char*, cols);
            memset((void*)vt->ScreenElem[i].lineimage, 0, cols * sizeof(char*));
            vt->ScreenElem[i].lineprop = New_N(l_prop, cols);
        }
    }
    vt->COLS = cols;

    {
        int i = 0;
        for (; i < rows; i++) {
            vt->ScreenImage[i] = &vt->ScreenElem[i];
            vt->ScreenImage[i]->lineprop[0] = S_EOL;
            vt->ScreenImage[i]->isdirty = 0;
        }
        for (; i < vt->ROWS; i++) {
            vt->ScreenImage[i] = &vt->ScreenElem[i];
            vt->ScreenImage[i]->lineprop[0] = S_EOL;
            vt->ScreenElem[i].isdirty = L_UNUSED;
        }
    }
}

void vt_move(struct VirtualTerm* vt, int line, int column)
{
    if (line >= 0 && line < vt->ROWS)
        vt->CurLine = line;
    if (column >= 0 && column < vt->COLS)
        vt->CurColumn = column;
}

static int
need_redraw(const char* c1, l_prop pr1, const char* c2, l_prop pr2)
{
    if (!c1 || !c2 || strcmp(c1, c2))
        return 1;
    if (*c1 == ' ')
        return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((pr1 ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

void vt_touch_column(struct VirtualTerm* vt, int col)
{
    if (col >= 0 && col < vt->COLS)
        vt->ScreenImage[vt->CurLine]->lineprop[col] |= S_DIRTY;
}

#define SPACE " "

void vt_addch(struct VirtualTerm* vt, char c)
{
    vt_addmch(vt, &c, 1);
}

void vt_addmch(struct VirtualTerm* vt, const char* pc, size_t len)
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

    if (vt->CurColumn == vt->COLS)
        vt_wrap(vt);
    if (vt->CurColumn >= vt->COLS)
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
    if (i < vt->COLS && (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], pc, vt->CurrentMode)) || ((pr[i] & S_UNDERLINE) && !(vt->CurrentMode & S_UNDERLINE)))) {
        vt_touch_line(vt);
        i++;
        if (i < vt->COLS) {
            vt_touch_column(vt, i);
            if (pr[i] & S_EOL) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < vt->COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
                    vt_touch_column(vt, i);
            }
        }
    }

    if (vt->CurColumn + width > vt->COLS) {
        vt_touch_line(vt);
        for (i = vt->CurColumn; i < vt->COLS; i++) {
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            vt_touch_column(vt, i);
        }
        vt_wrap(vt);
        if (vt->CurColumn + width > vt->COLS)
            return;
        p = vt->ScreenImage[vt->CurLine]->lineimage;
        pr = vt->ScreenImage[vt->CurLine]->lineprop;
    }
    if (CHMODE(pr[vt->CurColumn]) == C_WCHAR2) {
        vt_touch_line(vt);
        for (i = vt->CurColumn - 1; i >= 0; i--) {
            l_prop l = CHMODE(pr[i]);
            SETCH(p[i], SPACE, 1);
            SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
            vt_touch_column(vt, i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(vt->CurrentMode) != C_CTRL) {
        if (need_redraw(p[vt->CurColumn], pr[vt->CurColumn], pc, vt->CurrentMode)) {
            SETCH(p[vt->CurColumn], pc, len);
            SETPROP(pr[vt->CurColumn], vt->CurrentMode);
            vt_touch_line(vt);
            vt_touch_column(vt, vt->CurColumn);
            SETCHMODE(vt->CurrentMode, C_WCHAR2);
            for (i = vt->CurColumn + 1; i < vt->CurColumn + width; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[vt->CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
                vt_touch_column(vt, i);
            }
            for (; i < vt->COLS && CHMODE(pr[i]) == C_WCHAR2; i++) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
                vt_touch_column(vt, i);
            }
        }
        vt->CurColumn += width;
    } else if (c == '\t') {
        dest = (vt->CurColumn + vt->tab_step) / vt->tab_step * vt->tab_step;
        if (dest >= vt->COLS) {
            vt_wrap(vt);
            vt_touch_line(vt);
            dest = vt->tab_step;
            p = vt->ScreenImage[vt->CurLine]->lineimage;
            pr = vt->ScreenImage[vt->CurLine]->lineprop;
        }
        for (i = vt->CurColumn; i < dest; i++) {
            if (need_redraw(p[i], pr[i], SPACE, vt->CurrentMode)) {
                SETCH(p[i], SPACE, 1);
                SETPROP(pr[i], vt->CurrentMode);
                vt_touch_line(vt);
                vt_touch_column(vt, i);
            }
        }
        vt->CurColumn = i;
    } else if (c == '\n') {
        vt_wrap(vt);
    } else if (c == '\r') { /* Carriage return */
        vt->CurColumn = 0;
    } else if (c == '\b' && vt->CurColumn > 0) { /* Backspace */
        vt->CurColumn--;
        while (vt->CurColumn > 0 && CHMODE(pr[vt->CurColumn]) == C_WCHAR2)
            vt->CurColumn--;
    }
}

void vt_wrap(struct VirtualTerm* vt)
{
    if (vt->CurLine == vt->ROWS - 1)
        return;
    vt->CurLine++;
    vt->CurColumn = 0;
}

void vt_touch_line(struct VirtualTerm* vt)
{
    if (!(vt->ScreenImage[vt->CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < vt->COLS; i++)
            vt->ScreenImage[vt->CurLine]->lineprop[i] &= ~S_DIRTY;
        vt->ScreenImage[vt->CurLine]->isdirty |= L_DIRTY;
    }
}

void vt_clear(struct VirtualTerm* vt)
{
    vt_move(vt, 0, 0);
    struct scline** l = vt->ScreenImage;
    for (int i = 0; i < vt->ROWS; ++i, ++l) {
        (*l)->isdirty = 0;
        l_prop* p = (*l)->lineprop;
        for (int j = 0; j < vt->COLS; ++j, ++p) {
            *p = S_EOL;
        }
    }
    vt->CurrentMode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void vt_clrtoeol(struct VirtualTerm* vt)
{ /* Clear to the end of line */
    int i;
    l_prop* lprop = vt->ScreenImage[vt->CurLine]->lineprop;

    if (lprop[vt->CurColumn] & S_EOL)
        return;

    if (!(vt->ScreenImage[vt->CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || vt->ScreenImage[vt->CurLine]->eol > vt->CurColumn)
        vt->ScreenImage[vt->CurLine]->eol = vt->CurColumn;

    vt->ScreenImage[vt->CurLine]->isdirty |= L_CLRTOEOL;
    vt_touch_line(vt);
    for (i = vt->CurColumn; i < vt->COLS && !(lprop[i] & S_EOL); i++) {
        lprop[i] = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(struct VirtualTerm* vt)
{
    if (!(vt->CurrentMode & S_BCOLORED)) {
        vt_clrtoeol(vt);
        return;
    }
    int cli = vt->CurLine;
    int cco = vt->CurColumn;
    l_prop pr = vt->CurrentMode;
    vt->CurrentMode = (vt->CurrentMode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (int i = vt->CurColumn; i < vt->COLS; i++)
        vt_addch(vt, ' ');
    vt_move(vt, cli, cco);
    vt->CurrentMode = pr;
}

void vt_clrtoeolx(struct VirtualTerm* vt)
{
    clrtoeol_with_bcolor(vt);
}

typedef void (*ClearFunc)(struct VirtualTerm* vt);

static void vt_clrtobot_eol(struct VirtualTerm* vt, ClearFunc clrtoeol)
{
    int l = vt->CurLine;
    int c = vt->CurColumn;
    (*clrtoeol)(vt);
    vt->CurColumn = 0;
    vt->CurLine++;
    for (; vt->CurLine < vt->ROWS; vt->CurLine++)
        (*clrtoeol)(vt);
    vt->CurLine = l;
    vt->CurColumn = c;
}

void vt_clrtobot(struct VirtualTerm* vt)
{
    vt_clrtobot_eol(vt, vt_clrtoeol);
}

void vt_clrtobotx(struct VirtualTerm* vt)
{
    vt_clrtobot_eol(vt, vt_clrtoeolx);
}

void vt_addstr(struct VirtualTerm* vt, const char* s)
{
    while (*s != '\0') {
        int len = wtf_len((wc_uchar*)s);
        vt_addmch(vt, s, len);
        s += len;
    }
}

void vt_addnstr(struct VirtualTerm* vt, const char* s, int n)
{
    for (int i = 0; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        vt_addmch(vt, s, len);
        s += len;
        i += width;
    }
}

void vt_addnstr_sup(struct VirtualTerm* vt, const char* s, int n)
{
    int i = 0;
    for (; *s != '\0';) {
        int width = wtf_width((wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((wc_uchar*)s);
        vt_addmch(vt, s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        vt_addch(vt, ' ');
}

void vt_touch_cursor(struct VirtualTerm* vt)
{
    int i;
    vt_touch_line(vt);
    for (i = vt->CurColumn; i >= 0; i--) {
        vt_touch_column(vt, i);
        if (CHMODE(vt->ScreenImage[vt->CurLine]->lineprop[i]) != C_WCHAR2)
            break;
    }
    for (i = vt->CurColumn + 1; i < vt->COLS; i++) {
        if (CHMODE(vt->ScreenImage[vt->CurLine]->lineprop[i]) != C_WCHAR2)
            break;
        vt_touch_column(vt, i);
    }
}

void vt_addMChar(struct VirtualTerm* vt, char* p, Lineprop mode, size_t len, bool use_graphic)
{
    Lineprop m = CharEffect(mode);
    char c = *p;

    if (mode & PC_WCHAR2)
        return;
    vt_do_effects(vt, m);
    if (mode & PC_SYMBOL) {
        char** symbol;
        int w = (mode & PC_KANJI) ? 2 : 1;

        c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
        if (use_graphic && c < N_GRAPH_SYMBOL) {
            if (!graph_mode) {
                vt_graphstart(vt);
                graph_mode = true;
            }
            if (w == 2 && WcOption.use_wide)
                vt_addstr(vt, graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
            else
                vt_addstr(vt, graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
        } else {
            symbol = get_symbol(DisplayCharset, &w);
            vt_addstr(vt, symbol[(unsigned char)c % N_SYMBOL]);
        }
    } else if (mode & PC_CTRL) {
        switch (c) {
        case '\t':
            vt_addch(vt, c);
            break;
        case '\n':
            vt_addch(vt, ' ');
            break;
        case '\r':
            break;
        case DEL_CODE:
            vt_addstr(vt, "^?");
            break;
        default:
            vt_addch(vt, '^');
            vt_addch(vt, c + '@');
            break;
        }
    } else if (mode & PC_UNKNOWN) {
        char buf[5];
        sprintf(buf, "[%.2X]",
            (unsigned char)wtf_get_code((wc_uchar*)p) | 0x80);
        vt_addstr(vt, buf);
    } else
        vt_addmch(vt, p, len);
}

