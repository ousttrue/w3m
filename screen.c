#include "screen.h"
#include "Str.h"
#include "global.h"

#include "myctype.h"
#include "wc_util.h"
#include <libwc/wtf.h>
#include <string.h>
#include <stdlib.h>

static const uint8_t* SPACE = (const uint8_t*)" ";

int sc_curline() { return CurLine; }
int sc_curcol() { return CurColumn; }

struct CellMode CurrentMode = { 0 };

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

void sc_touch_line(void)
{
    if (!sc_getline(CurLine)->isdirty.L_DIRTY) {
        int i;
        for (i = 0; i < COLS; i++)
            sc_getline(CurLine)->cells[i].mode.S_DIRTY = false;
        sc_getline(CurLine)->isdirty.L_DIRTY = true;
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
