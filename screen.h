#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* struct ScreenLine properties */
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

#define S_BCOLORED 0xf000

#define S_GRAPHICS 0x10

#define S_DIRTY 0x20

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

#define M_SPACE (S_SCREENPROP | S_COLORED | S_BCOLORED | S_GRAPHICS)
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))
#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))

/* Line status */
#define L_DIRTY 0x01
#define L_UNUSED 0x02
#define L_NEED_CE 0x04
#define L_CLRTOEOL 0x08

#define ISDIRTY(d) ((d) & L_DIRTY)
#define ISUNUSED(d) ((d) & L_UNUSED)
#define NEED_CE(d) ((d) & L_NEED_CE)

struct ScreenLine {
    char** lineimage;
    uint16_t* lineprop;
    short isdirty;
    short eol;
};

struct Screen {
    struct ScreenLine** ScreenImage;
    int lines;
    int cols;
    int CurLine;
    int CurColumn;
    int mode;
};
struct Screen getScreen();
void set_screen_mode(uint16_t mode);

void setupscreen(int lines, int cols);
void clear(void);
bool is_need_redraw(const char* c1, uint16_t pr1, const char* c2, uint16_t pr2);
void move(int line, int column);
void touch_line(void);
void touch_column(int);
void touch_cursor(void);

void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);

void addmch(const char* p, size_t len);
inline static void addch(char c)
{
    addmch(&c, 1);
}
void standout(void);
void standend(void);
void toggle_stand(void);
void bold(void);
void boldend(void);
void underline(void);
void underlineend(void);
void graphstart(void);
void graphend(void);
void setfcolor(int color);
void setbcolor(int color);
