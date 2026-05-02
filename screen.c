#include "screen.h"
#include "Str.h"
#include "global.h"

#include "myctype.h"
#include "wc_util.h"
#include <libwc/wtf.h>
#include <string.h>
#include <stdlib.h>

static const uint8_t* SPACE = (const uint8_t*)" ";

static int tab_step = 8;
static int CurLine = 0;
static int CurColumn = 0;

int sc_curline() { return CurLine; }
int sc_curcol() { return CurColumn; }

static struct CellMode CurrentMode = { 0 };

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
        sc_getline(CurLine)->cells[col].mode.S_DIRTY = true;
}

static void sc_touch_line(void)
{
    if (!(sc_getline(CurLine)->isdirty & L_DIRTY)) {
        int i;
        for (i = 0; i < COLS; i++)
            sc_getline(CurLine)->cells[i].mode.S_DIRTY = false;
        sc_getline(CurLine)->isdirty |= L_DIRTY;
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

    struct Cell* line = sc_getline(CurLine)->cells;
    if (line[CurColumn].mode.S_EOL) {
        if (src[0] == ' ') {
            if (!CurrentMode.prop.S_STANDOUT
                && !CurrentMode.prop.S_BOLD
                && !CurrentMode.prop.S_UNDERLINE
                && !CurrentMode.prop.S_GRAPHICS
                && CurrentMode.fg == ANSI_TERM
                && CurrentMode.bg == ANSI_TERM) {
                CurColumn++;
                return;
            }
        }
        for (i = CurColumn; i >= 0 && (line[i].mode.S_EOL); i--) {
            struct CellMode mode = line[i].mode;
            mode.prop = (struct CellProperty) { 0 };
            mode.fg = ANSI_TERM;
            mode.bg = ANSI_TERM;
            mode.charmode = C_ASCII;
            sc_cell_set(&line[i], SPACE, 1, mode);
        }
    }

    if (src[0] == '\t' || src[0] == '\n' || src[0] == '\r' || src[0] == '\b') {
        CurrentMode.charmode = C_ASCII;
        CurrentMode.C_CTRL = true;
    } else if (len > 1) {
        CurrentMode.charmode = C_WCHAR1;
        CurrentMode.C_CTRL = false;
    } else if (!IS_CNTRL(src[0])) {
        CurrentMode.charmode = C_ASCII;
        CurrentMode.C_CTRL = false;
    } else {
        return;
    }

    /* Required to erase bold or underlined character for some * terminal
     * emulators. */
    i = CurColumn + width - 1;
    if (i < COLS && (((line[i].mode.prop.S_BOLD) && sc_cell_need_redraw(&line[i], (CellCharBytes)pc, CurrentMode)) || ((line[i].mode.prop.S_UNDERLINE) && !(CurrentMode.prop.S_UNDERLINE)))) {
        sc_touch_line();
        i++;
        if (i < COLS) {
            touch_column(i);
            if (line[i].mode.S_EOL) {
                struct CellMode mode = line[i].mode;
                mode.prop = (struct CellProperty) { 0 };
                mode.fg = ANSI_TERM;
                mode.bg = ANSI_TERM;
                mode.charmode = C_ASCII;
                sc_cell_set(&line[i], SPACE, 1, mode);
            } else {
                for (i++; i < COLS && line[i].mode.charmode == C_WCHAR2; i++)
                    touch_column(i);
            }
        }
    }

    if (CurColumn + width > COLS) {
        sc_touch_line();
        for (i = CurColumn; i < COLS; i++) {
            struct CellMode mode = line[i].mode;
            mode.charmode = C_ASCII;
            sc_cell_set(&line[i], SPACE, 1, mode);
            touch_column(i);
        }
        sc_wrap();
        if (CurColumn + width > COLS)
            return;
        line = sc_getline(CurLine)->cells;
    }
    if (line[CurColumn].mode.charmode == C_WCHAR2) {
        sc_touch_line();
        for (i = CurColumn - 1; i >= 0; i--) {
            enum CharMode l = line[i].mode.charmode;
            struct CellMode mode = line[i].mode;
            mode.charmode = C_ASCII;
            sc_cell_set(&line[i], SPACE, 1, mode);
            touch_column(i);
            if (l != C_WCHAR2)
                break;
        }
    }
    if (!CurrentMode.C_CTRL) {
        if (sc_cell_need_redraw(&line[CurColumn], (CellCharBytes)pc, CurrentMode)) {
            sc_cell_set(&line[CurColumn], (CellCharBytes)pc, len, CurrentMode);
            sc_touch_line();
            touch_column(CurColumn);
            CurrentMode.charmode = C_WCHAR2;
            for (i = CurColumn + 1; i < CurColumn + width; i++) {
                struct CellMode mode = line[CurColumn].mode;
                mode.charmode = C_WCHAR2;
                sc_cell_set(&line[i], SPACE, 1, mode);
                touch_column(i);
            }
            for (; i < COLS && line[i].mode.charmode == C_WCHAR2; i++) {
                struct CellMode mode = line[i].mode;
                mode.charmode = C_ASCII;
                sc_cell_set(&line[i], SPACE, 1, mode);
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
            line = sc_getline(CurLine)->cells;
        }
        for (i = CurColumn; i < dest; i++) {
            if (sc_cell_need_redraw(&line[i], SPACE, CurrentMode)) {
                sc_cell_set(&line[i], SPACE, 1, CurrentMode);
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
        while (CurColumn > 0 && line[CurColumn].mode.charmode == C_WCHAR2)
            CurColumn--;
    }
}

void sc_standout(void)
{
    CurrentMode.prop.S_STANDOUT = true;
}

void sc_standend(void)
{
    CurrentMode.prop.S_STANDOUT = false;
}

void sc_toggle_stand(void)
{
    struct Cell* line = sc_getline(CurLine)->cells;
    line[CurColumn].mode.prop.S_STANDOUT = !line[CurColumn].mode.prop.S_STANDOUT;
    if (line[CurColumn].mode.charmode != C_WCHAR2) {
        for (int i = CurColumn + 1; line[i].mode.charmode == C_WCHAR2; i++)
            line[i].mode.prop.S_STANDOUT = !line[i].mode.prop.S_STANDOUT;
    }
}

void sc_bold(void)
{
    CurrentMode.prop.S_BOLD = true;
}

void sc_boldend(void)
{
    CurrentMode.prop.S_BOLD = false;
}

void sc_underline(void)
{
    CurrentMode.prop.S_UNDERLINE = true;
}

void sc_underlineend(void)
{
    CurrentMode.prop.S_UNDERLINE = false;
}

void sc_graphstart(void)
{
    CurrentMode.prop.S_GRAPHICS = true;
}

void sc_graphend(void)
{
    CurrentMode.prop.S_GRAPHICS = false;
}

void sc_setfcolor(enum AnsiColor color)
{
    CurrentMode.fg = color;
}

const char* sc_color_seq(enum AnsiColor colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", colmode + (highIntensityColors ? 90 : 30));
    return seqbuf;
}

void sc_setbcolor(enum AnsiColor color)
{
    CurrentMode.bg = color;
}

const char* sc_bcolor_seq(enum AnsiColor colmode)
{
    static char seqbuf[32];
    sprintf(seqbuf, "\033[%dm", colmode + 40);
    return seqbuf;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
static void sc_clrtoeol(void)
{ /* Clear to the end of line */
    struct Cell* line = sc_getline(CurLine)->cells;

    if (line[CurColumn].mode.S_EOL)
        return;

    if (!(sc_getline(CurLine)->isdirty & (L_NEED_CE | L_CLRTOEOL)) || sc_getline(CurLine)->eol > CurColumn)
        sc_getline(CurLine)->eol = CurColumn;

    sc_getline(CurLine)->isdirty |= L_CLRTOEOL;
    sc_touch_line();
    for (int i = CurColumn; i < COLS && !line[i].mode.S_EOL; i++) {
        line[i].mode.S_EOL = true;
        line[i].mode.S_DIRTY = true;
    }
}

static void
clrtoeol_with_bcolor(void)
{

    if (CurrentMode.bg == ANSI_TERM) {
        sc_clrtoeol();
        return;
    }
    int cli = CurLine;
    int cco = CurColumn;
    struct CellMode pr = CurrentMode;
    CurrentMode.prop = (struct CellProperty) { 0 };
    CurrentMode.charmode = C_ASCII;
    CurrentMode.fg = ANSI_TERM;
    CurrentMode.bg = ANSI_TERM;
    for (int i = CurColumn; i < COLS; i++)
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
