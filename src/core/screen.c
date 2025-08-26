#include "screen.h"
#include "term_size.h"
#include "term_entry.h"
#include "graphicchar.h"
#include "term_renderer.h"
#include "fm.h"
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

void setbcolor(struct VirtualTerm* vt, int color)
{
    vt->CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        vt->CurrentMode |= (((color & 7) | 8) << 12);
}

static void MOVE(const struct Writer* writer, int line, int column)
{
    putsWriter(writer, getMoveXY(column, line));
}

// Screen to STDOUT
void refresh(const struct Writer* writer)
{
    struct VirtualTerm* vt = getScreen();
    struct Frame* frame = New(struct Frame);
    frame->lines = getLines();
    frame->cols = getCols();
    frame->cells = New_N(struct Cell, frame->lines * frame->cols);
    struct Cell* cell = frame->cells;
    wc_putc_init(InnerCharset, DisplayCharset);
    for (int y = 0; y < frame->lines; ++y) {
        Screen* l = vt->ScreenImage[y];
        for (int x = 0; x < frame->cols; ++x, ++cell) {
            cell->prop = l->lineprop[x];
            if (cell->prop & S_EOL || CHMODE(cell->prop) == C_WCHAR2) {
                memset(cell->str, 0, sizeof(cell->str));
            } else {
                struct ArrayInfo info = {
                    .buf = cell->str,
                    .len = sizeof(cell->str),
                    .pos = 0,
                };
                struct Writer w;
                makeArrayWriter(&w, &info);
                wc_putc(&w, l->lineimage[x]);
            }
        }
    }
    wc_putc_end(writer);

    wc_putc_init(InnerCharset, DisplayCharset);
    // for (int line = 0; line <= getLines() - 1; line++) {
    //     refreshLine(writer, vt, line);
    // }
    refreshFrame(writer, frame);
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
