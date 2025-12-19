#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_SPACE " "

enum ScreenCellProperty : uint16_t {
    // Screen properties
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

// Line status
enum ScreenLineFlags : uint16_t {
    L_DIRTY = 0x01,
    L_UNUSED = 0x02,
    L_NEED_CE = 0x04,
    L_CLRTOEOL = 0x08,
};

enum ScreenCellProperty CHAR_MODE(enum ScreenCellProperty c);

struct ScreenCell {
    char str[8];
    enum ScreenCellProperty prop;
};

void SET_CHAR(struct ScreenCell* p, const char* ch, size_t len);
void SET_CHAR_MODE(enum ScreenCellProperty* var, enum ScreenCellProperty mode);
void SET_PROP(struct ScreenCell* p, enum ScreenCellProperty prop);

struct ScreenLine {
    struct ScreenCell* cells;
    enum ScreenLineFlags isdirty;
    size_t eol;
};

struct Screen {
    size_t line_count;
    size_t line_capacity;
    size_t col_count;
    size_t col_capacity;
    struct ScreenLine* lines;
    size_t y;
    size_t x;
    int tab_step;
    enum ScreenCellProperty mode;
};

struct Screen* screen_get();

bool screen_need_redraw(char* c1, enum ScreenCellProperty pr1, char* c2, enum ScreenCellProperty pr2);
void screen_setup(int lines, int cols);
void screen_move(int line, int column);
void screen_addmch(const char* p, size_t len, int width);
static inline void screen_add_whitespace()
{
    const char* white_space = " ";
    screen_addmch(white_space, 1, 1);
}
void screen_add_tab();
inline static void screen_addch(char c, int width)
{
    screen_addmch(&c, 1, width);
}
void screen_wrap(void);
void screen_touch_line(void);
void screen_standout(void);
void screen_standend(void);
void screen_toggle_stand(void);
void screen_bold(void);
void screen_boldend(void);
void screen_underline(void);
void screen_underlineend(void);
void screen_graphstart(void);
void screen_graphend(void);
void screen_setfcolor(int color);
void screen_setbcolor(int color);
void screen_clear(void);
void screen_clrtoeol(void);
void screen_clrtoeolx(void);
void screen_clrtobot(void);
void screen_clrtobotx(void);
void screen_touch_cursor(void);
void screen_touch_column(int col);
