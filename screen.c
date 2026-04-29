#include "screen.h"
#include "Str.h"
#include "global.h"
#include "alloc.h"
#include "myctype.h"
#include "wc_util.h"
#include <libwc/wtf.h>
#include <string.h>

struct TermInfo terminfo;

#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))

static void setCell(struct Cell* cell, CellCharBytes ch, size_t len, enum CellProperty prop)
{
    cell->bytes = New_Reuse(uint8_t, cell->bytes, len + 1);
    strncpy((char*)cell->bytes, (const char*)ch, len + 1);
    cell->prop = (cell->prop & S_DIRTY) | prop;
}

bool sc_need_redraw(const struct Cell* cell, const CellCharBytes c2, enum CellProperty pr2)
{
    if (!cell->bytes || !c2 || strcmp((const char*)cell->bytes, (const char*)c2))
        return 1;
    if (cell->bytes[0] == ' ')
        return (cell->prop ^ pr2) & M_SPACE & ~S_DIRTY;

    if ((cell->prop ^ pr2) & ~S_DIRTY)
        return 1;

    return 0;
}

static int max_LINES = 0, max_COLS = 0;
static int tab_step = 8;
static int CurLine, CurColumn;

int sc_curline() { return CurLine; }
int sc_curcol() { return CurColumn; }

static struct ScreenLine *ScreenElem = NULL, **ScreenImage = NULL;
struct ScreenLine** sc_lines()
{
    return ScreenImage;
}

static enum CellProperty CurrentMode = 0;

static uint8_t*
skip_gif_header(uint8_t* p)
{
    /* Header */
    p += 10;

    if (*(p) & 0x80) {
        p += (3 * (2 << ((*p) & 0x7)));
    }
    p += 3;

    return p;
}

void sc_init(void)
{
    if (LINES + 1 > max_LINES) {
        max_LINES = LINES + 1;
        max_COLS = 0;
        ScreenElem = New_N(struct ScreenLine, max_LINES);
        ScreenImage = New_N(struct ScreenLine*, max_LINES);
    }
    if (COLS + 1 > max_COLS) {
        max_COLS = COLS + 1;
        for (int i = 0; i < max_LINES; i++) {
            ScreenElem[i].cells = New_N(struct Cell, max_COLS);
            memset(ScreenElem[i].cells, 0, max_COLS * sizeof(struct Cell));
        }
    }

    int i = 0;
    for (; i < LINES; i++) {
        ScreenImage[i] = &ScreenElem[i];
        ScreenImage[i]->cells[0].prop = S_EOL;
        ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++) {
        ScreenElem[i].isdirty = L_UNUSED;
    }

    sc_clear();
}

void sc_move(int line, int column)
{
    if (line >= 0 && line < LINES)
        CurLine = line;
    if (column >= 0 && column < COLS)
        CurColumn = column;
}

void sc_addch(uint8_t c)
{
    sc_addmch(&c, 1);
}

static void touch_column(int col)
{
    if (col >= 0 && col < COLS)
        ScreenImage[CurLine]->cells[col].prop |= S_DIRTY;
}

static void sc_touch_line(void)
{
    if (!(ScreenImage[CurLine]->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < COLS; i++)
            ScreenImage[CurLine]->cells[i].prop &= ~S_DIRTY;
        ScreenImage[CurLine]->isdirty |= L_DIRTY;
    }
}

static void sc_wrap(void)
{
    if (CurLine == (LINES - 1))
        return;
    CurLine++;
    CurColumn = 0;
}

void sc_addmch(const uint8_t* src, size_t len)
{
    int dest, i;
    static Str tmp = NULL;
    int width = wtf_width(WcOption, src[0]);

    if (tmp == NULL)
        tmp = Strnew();
    Strcopy_charp_n(tmp, (const char*)src, len);
    char* pc = tmp->ptr;

    if (CurColumn == COLS)
        sc_wrap();
    if (CurColumn >= COLS)
        return;

    struct Cell* line = ScreenImage[CurLine]->cells;
    if (line[CurColumn].prop & S_EOL) {
        if (src[0] == ' ' && !(CurrentMode & M_SPACE)) {
            CurColumn++;
            return;
        }
        for (i = CurColumn; i >= 0 && (line[i].prop & S_EOL); i--) {
            setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & M_CEOL) | C_ASCII);
        }
    }

    if (src[0] == '\t' || src[0] == '\n' || src[0] == '\r' || src[0] == '\b')
        SETCHMODE(CurrentMode, C_CTRL);
    else if (len > 1)
        SETCHMODE(CurrentMode, C_WCHAR1);
    else if (!IS_CNTRL(src[0]))
        SETCHMODE(CurrentMode, C_ASCII);
    else
        return;

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = CurColumn + width - 1;
    if (i < COLS && (((line[i].prop & S_BOLD) && sc_need_redraw(&line[i], (CellCharBytes)pc, CurrentMode)) || ((line[i].prop & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
        sc_touch_line();
        i++;
        if (i < COLS) {
            touch_column(i);
            if (line[i].prop & S_EOL) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & M_CEOL) | C_ASCII);
            } else {
                for (i++; i < COLS && CHMODE(line[i].prop) == C_WCHAR2; i++)
                    touch_column(i);
            }
        }
    }

    if (CurColumn + width > COLS) {
        sc_touch_line();
        for (i = CurColumn; i < COLS; i++) {
            setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
        }
        sc_wrap();
        if (CurColumn + width > COLS)
            return;
        line = ScreenImage[CurLine]->cells;
    }
    if (CHMODE(line[CurColumn].prop) == C_WCHAR2) {
        sc_touch_line();
        for (i = CurColumn - 1; i >= 0; i--) {
            enum CellProperty l = CHMODE(line[i].prop);
            setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
            touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (CHMODE(CurrentMode) != C_CTRL) {
        if (sc_need_redraw(&line[CurColumn], (CellCharBytes)pc, CurrentMode)) {
            setCell(&line[CurColumn], (CellCharBytes)pc, len, CurrentMode);
            sc_touch_line();
            touch_column(CurColumn);
            SETCHMODE(CurrentMode, C_WCHAR2);
            for (i = CurColumn + 1; i < CurColumn + width; i++) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, (line[CurColumn].prop & ~C_WHICHCHAR) | C_WCHAR2);
                touch_column(i);
            }
            for (; i < COLS && CHMODE(line[i].prop) == C_WCHAR2; i++) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, (line[i].prop & ~C_WHICHCHAR) | C_ASCII);
                touch_column(i);
            }
        }
        CurColumn += width;
    } else if (src[0] == '\t') {
        dest = (CurColumn + tab_step) / tab_step * tab_step;
        if (dest >= COLS) {
            sc_wrap();
            sc_touch_line();
            dest = tab_step;
            line = ScreenImage[CurLine]->cells;
        }
        for (i = CurColumn; i < dest; i++) {
            if (sc_need_redraw(&line[i], (CellCharBytes)SPACE, CurrentMode)) {
                setCell(&line[i], (CellCharBytes)SPACE, 1, CurrentMode);
                sc_touch_line();
                touch_column(i);
            }
        }
        CurColumn = i;
    } else if (src[0] == '\n') {
        sc_wrap();
    } else if (src[0] == '\r') { /* Carriage return */
        CurColumn = 0;
    } else if (src[0] == '\b' && CurColumn > 0) { /* Backspace */
        CurColumn--;
        while (CurColumn > 0 && CHMODE(line[CurColumn].prop) == C_WCHAR2)
            CurColumn--;
    }
}

void sc_standout(void)
{
    CurrentMode |= S_STANDOUT;
}

void sc_standend(void)
{
    CurrentMode &= ~S_STANDOUT;
}

void sc_toggle_stand(void)
{
    struct Cell* line = ScreenImage[CurLine]->cells;
    line[CurColumn].prop ^= S_STANDOUT;
    if (CHMODE(line[CurColumn].prop) != C_WCHAR2) {
        for (int i = CurColumn + 1; CHMODE(line[i].prop) == C_WCHAR2; i++)
            line[i].prop ^= S_STANDOUT;
    }
}

void sc_bold(void)
{
    CurrentMode |= S_BOLD;
}

void sc_boldend(void)
{
    CurrentMode &= ~S_BOLD;
}

void sc_underline(void)
{
    CurrentMode |= S_UNDERLINE;
}

void sc_underlineend(void)
{
    CurrentMode &= ~S_UNDERLINE;
}

void sc_graphstart(void)
{
    CurrentMode |= S_GRAPHICS;
}

void sc_graphend(void)
{
    CurrentMode &= ~S_GRAPHICS;
}

void sc_setfcolor(int color)
{
    CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 8);
}

const char* sc_color_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + (highIntensityColors ? 90 : 30));
    return seqbuf;
}

void sc_setbcolor(int color)
{
    CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        CurrentMode |= (((color & 7) | 8) << 12);
}

const char* sc_bcolor_seq(int colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", ((colmode >> 12) & 7) + 40);
    return seqbuf;
}

void sc_clear(void)
{
    sc_move(0, 0);
    for (int i = 0; i < LINES; i++) {
        ScreenImage[i]->isdirty = 0;
        struct Cell* p = ScreenImage[i]->cells;
        for (int j = 0; j < COLS; j++) {
            p[j].prop = S_EOL;
        }
    }
    CurrentMode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
static void sc_clrtoeol(void)
{ /* Clear to the end of line */
    struct Cell* line = ScreenImage[CurLine]->cells;

    if (line[CurColumn].prop & S_EOL)
        return;

    if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) || ScreenImage[CurLine]->eol > CurColumn)
        ScreenImage[CurLine]->eol = CurColumn;

    ScreenImage[CurLine]->isdirty |= L_CLRTOEOL;
    sc_touch_line();
    for (int i = CurColumn; i < COLS && !(line[i].prop & S_EOL); i++) {
        line[i].prop = S_EOL | S_DIRTY;
    }
}

static void
clrtoeol_with_bcolor(void)
{
    int i, cli, cco;
    enum CellProperty pr;

    if (!(CurrentMode & S_BCOLORED)) {
        sc_clrtoeol();
        return;
    }
    cli = CurLine;
    cco = CurColumn;
    pr = CurrentMode;
    CurrentMode = (CurrentMode & (M_CEOL | S_BCOLORED)) | C_ASCII;
    for (i = CurColumn; i < COLS; i++)
        sc_addch(' ');
    sc_move(cli, cco);
    CurrentMode = pr;
}

void sc_clrtoeolx(void)
{
    clrtoeol_with_bcolor();
}

static void
clrtobot_eol(void (*clrtoeol)())
{
    int l = CurLine;
    int c = CurColumn;
    clrtoeol();
    CurColumn = 0;
    CurLine++;
    for (; CurLine < LINES; CurLine++)
        clrtoeol();
    CurLine = l;
    CurColumn = c;
}

void sc_clrtobotx(void)
{
    clrtobot_eol(sc_clrtoeolx);
}

void sc_addstr(const char* s)
{
    while (*s != '\0') {
        int len = wtf_len((wc_uchar*)s);
        sc_addmch((CellCharBytes)s, len);
        s += len;
    }
}

void sc_addnstr(const char* s, int n)
{
    for (int i = 0; *s != '\0';) {
        int width = wtf_width(WcOption, *(const wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((const wc_uchar*)s);
        sc_addmch((CellCharBytes)s, len);
        s += len;
        i += width;
    }
}

void sc_addnstr_sup(const char* s, int n)
{
    int i = 0;
    for (; *s != '\0';) {
        int width = wtf_width(WcOption, *(const wc_uchar*)s);
        if (i + width > n)
            break;
        int len = wtf_len((const wc_uchar*)s);
        sc_addmch((CellCharBytes)s, len);
        s += len;
        i += width;
    }
    for (; i < n; i++)
        sc_addch(' ');
}
