#pragma once
#include "terminfo_entry.h"
#include <stddef.h>
#include <stdint.h>

extern struct TermInfo terminfo;

enum CellProperty : uint16_t {
    // struct ScreenLine properties
    S_SCREENPROP = 0x0f,
    S_NORMAL = 0x00,
    S_STANDOUT = 0x01,
    S_UNDERLINE = 0x02,
    S_BOLD = 0x04,
    S_EOL = 0x08,

    // Sort of Character
    C_WHICHCHAR = 0xc0,
    C_ASCII = 0x00,
    C_WCHAR1 = 0x40,
    C_WCHAR2 = 0x80,
    C_CTRL = 0xc0,

    // Charactor Color
    COL_FCOLOR = 0xf00,
    COL_FBLACK = 0x800,
    COL_FRED = 0x900,
    COL_FGREEN = 0xa00,
    COL_FYELLOW = 0xb00,
    COL_FBLUE = 0xc00,
    COL_FMAGENTA = 0xd00,
    COL_FCYAN = 0xe00,
    COL_FWHITE = 0xf00,
    COL_FTERM = 0x000,

    S_COLORED = 0xf00,

    // Background Color
    COL_BCOLOR = 0xf000,
    COL_BBLACK = 0x8000,
    COL_BRED = 0x9000,
    COL_BGREEN = 0xa000,
    COL_BYELLOW = 0xb000,
    COL_BBLUE = 0xc000,
    COL_BMAGENTA = 0xd000,
    COL_BCYAN = 0xe000,
    COL_BWHITE = 0xf000,
    COL_BTERM = 0x0000,

    S_BCOLORED = 0xf000,

    S_GRAPHICS = 0x10,

    S_DIRTY = 0x20,
};
#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))
#define SPACE " "
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_BCOLORED | S_GRAPHICS)
static inline enum CellProperty CHMODE(enum CellProperty c) { return ((c)&C_WHICHCHAR); }

enum LineFlags : uint16_t {
    L_DIRTY = 0x01,
    L_UNUSED = 0x02,
    L_NEED_CE = 0x04,
    L_CLRTOEOL = 0x08,
};

typedef const uint8_t* CellCharBytes;

struct Cell {
    CellCharBytes bytes;
    enum CellProperty prop;
};

struct ScreenLine {
    struct Cell* cells;
    enum LineFlags isdirty;
    short eol;
};

void sc_init(void);
int sc_curline();
int sc_curcol();
struct ScreenLine** sc_lines();

void sc_move(int line, int column);
void sc_addmch(const uint8_t* p, size_t len);
void sc_addch(uint8_t c);
void sc_toggle_stand(void);
void sc_standout(void);
void sc_standend(void);
void sc_bold(void);
void sc_boldend(void);
void sc_underline(void);
void sc_underlineend(void);
void sc_graphstart(void);
void sc_graphend(void);
void sc_setfcolor(int color);
void sc_setbcolor(int color);
void sc_clear(void);
void sc_clrtoeolx(void);
void sc_clrtobotx(void);
void sc_addstr(const char* s);
void sc_addnstr(const char* s, int n);
void sc_addnstr_sup(const char* s, int n);

static inline void sc_mvaddnstr(int y, int x, const char* str, int n)
{
    sc_move(y, x);
    sc_addnstr_sup(str, n);
}

static inline void sc_mvaddch(int y, int x, int c)
{
    sc_move(y, x);
    sc_addch(c);
}

static inline void sc_mvaddstr(int y, int x, const char* str)
{
    sc_move(y, x);
    sc_addstr(str);
}

bool sc_need_redraw(const struct Cell* cell, const CellCharBytes c2, enum CellProperty pr2);
const char* sc_color_seq(int colmode);
const char* sc_bcolor_seq(int colmode);
