#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_SPACE " "

/* Screen properties */
#define S_SCREENPROP 0x0f
#define S_NORMAL 0x00
#define S_STANDOUT 0x01
#define S_UNDERLINE 0x02
#define S_BOLD 0x04
#define S_EOL 0x08

/* Sort of Character */
#define C_WHICHCHAR 0xc0
#define C_ASCII 0x00
#define C_WCHAR1 0x40
#define C_WCHAR2 0x80
#define C_CTRL 0xc0

#define CHMODE(c) ((c) & C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define SETCH(var, ch, len) ((var) = New_Reuse(char, (var), (len) + 1), \
    strncpy((var), (ch), (len + 1)))

/* Charactor Color */
#define COL_FCOLOR 0xf00
#define COL_FBLACK 0x800
#define COL_FRED 0x900
#define COL_FGREEN 0xa00
#define COL_FYELLOW 0xb00
#define COL_FBLUE 0xc00
#define COL_FMAGENTA 0xd00
#define COL_FCYAN 0xe00
#define COL_FWHITE 0xf00
#define COL_FTERM 0x000

#define S_COLORED 0xf00

/* Background Color */
#define COL_BCOLOR 0xf000
#define COL_BBLACK 0x8000
#define COL_BRED 0x9000
#define COL_BGREEN 0xa000
#define COL_BYELLOW 0xb000
#define COL_BBLUE 0xc000
#define COL_BMAGENTA 0xd000
#define COL_BCYAN 0xe000
#define COL_BWHITE 0xf000
#define COL_BTERM 0x0000

#define S_BCOLORED 0xf000

#define S_GRAPHICS 0x10

#define S_DIRTY 0x20

#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))

/* Line status */
#define L_DIRTY 0x01
#define L_UNUSED 0x02
#define L_NEED_CE 0x04
#define L_CLRTOEOL 0x08

#define ISDIRTY(d) ((d) & L_DIRTY)
#define ISUNUSED(d) ((d) & L_UNUSED)
#define NEED_CE(d) ((d) & L_NEED_CE)

typedef unsigned short l_prop;

struct ScreenLine {
    char** lineimage;
    l_prop* lineprop;
    short isdirty;
    short eol;
};

struct Screen {
    int lines;
    int lines_capacity;
    int cols;
    int cols_capacity;
    int tab_step;
    int y;
    int x;
    struct ScreenLine* cells;
    l_prop mode;
};

struct Screen* screen_get();

bool screen_need_redraw(char* c1, l_prop pr1, char* c2, l_prop pr2);
void screen_setup(int lines, int cols);
void screen_move(int line, int column);
void screen_addmch(char* p, size_t len);
inline static void addch(char c)
{
    screen_addmch(&c, 1);
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
void screen_addstr(char* s);
void screen_addnstr(char* s, int n);
void screen_addnstr_sup(char* s, int n);
void screen_touch_cursor(void);
void screen_touch_column(int col);
