#pragma once
#include "terminfo_entry.h"
#include <stddef.h>
#include <stdint.h>

extern struct TermInfo terminfo;

struct CellProperty {
    bool S_SCREENPROP;
    bool S_NORMAL;
    bool S_STANDOUT;
    bool S_UNDERLINE;
    bool S_BOLD;
    bool S_GRAPHICS;
};

enum CharMode {
    C_ASCII,
    C_WCHAR1,
    C_WCHAR2,
};

enum AnsiColor {
    ANSI_COLOR,
    ANSI_BLACK,
    ANSI_RED,
    ANSI_GREEN,
    ANSI_YELLOW,
    ANSI_BLUE,
    ANSI_MAGENTA,
    ANSI_CYAN,
    ANSI_WHITE,
    ANSI_TERM,
};

struct CellMode {
    struct CellProperty prop;
    enum CharMode charmode;
    enum AnsiColor fg;
    enum AnsiColor bg;
    bool S_DIRTY;
    bool S_EOL;
    bool C_CTRL;
};

static inline bool is_mend(struct CellMode mode)
{
    if (mode.prop.S_STANDOUT | mode.prop.S_UNDERLINE | mode.prop.S_BOLD | mode.prop.S_GRAPHICS) {
        return true;
    }
    if (mode.fg != ANSI_TERM || mode.bg != ANSI_TERM) {
        return true;
    }
    return false;
}

static inline void remove_mend(struct CellMode* mode)
{
    mode->prop = (struct CellProperty) { };
    mode->fg = ANSI_TERM;
    mode->bg = ANSI_TERM;
}

enum LineFlags : uint16_t {
    L_DIRTY = 0x01,
    L_UNUSED = 0x02,
    L_NEED_CE = 0x04,
    L_CLRTOEOL = 0x08,
};

typedef const uint8_t* CellCharBytes;

struct Cell {
    CellCharBytes bytes;
    struct CellMode mode;
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
void sc_setfcolor(enum AnsiColor color);
void sc_setbcolor(enum AnsiColor color);
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

bool sc_need_redraw(const struct Cell* cell, const CellCharBytes c2, struct CellMode pr2);
const char* sc_color_seq(enum AnsiColor colmode);
const char* sc_bcolor_seq(enum AnsiColor colmode);
