#pragma once
#include <stddef.h>
#include <stdint.h>

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

enum LineFlags : uint16_t {
    L_DIRTY = 0x01,
    L_UNUSED = 0x02,
    L_NEED_CE = 0x04,
    L_CLRTOEOL = 0x08,
};

typedef uint8_t* CellCharBytes;

struct Cell {
    CellCharBytes bytes;
    enum CellProperty prop;
};

void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(const char* url, int x, int y, int w, int h);
void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);

void mouse_active();
void mouse_inactive();
void mouse_end();
void reset_tty(void);
void set_int(void);
void setupscreen(void);
int initscr(void);
void move(int line, int column);
void addmch(const uint8_t* p, size_t len);
void addch(uint8_t c);
void wrap(void);
void touch_line(void);
void standout(void);
void standend(void);
void bold(void);
void boldend(void);
void underline(void);
void underlineend(void);
void graphstart(void);
void graphend(void);
int graph_ok(void);
void setfcolor(int color);
void setbcolor(int color);
void refresh(void);
void clear(void);
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(const char* s);
void addnstr(const char* s, int n);
void addnstr_sup(const char* s, int n);

void touch_cursor(void);
