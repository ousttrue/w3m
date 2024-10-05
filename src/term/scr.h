#pragma once
#include "text/utf8.h"

#define CHMODE(c) ((c) & C_WHICHCHAR)

enum CharactorColor {
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
};

enum LineStatus {
  L_DIRTY = 0x01,
  L_UNUSED = 0x02,
  L_NEED_CE = 0x04,
  L_CLRTOEOL = 0x08,
};

enum ScreenProperties {
  SCREEN_NORMAL = 0x00,
  SCREEN_STANDOUT = 0x01,
  SCREEN_UNDERLINE = 0x02,
  SCREEN_BOLD = 0x04,
  SCREEN_EOL = 0x08,
  SCREEN_SCREENPROP = 0x0f,

  SCREEN_GRAPHICS = 0x10,
  SCREEN_DIRTY = 0x20,
  C_WCHAR1 = 0x40,
  C_WCHAR2 = 0x80,
  C_ASCII = 0x00,
  C_WHICHCHAR = 0xc0,
  C_CTRL = 0xc0,

  SCREEN_COLORED = 0xf00,

  M_SPACE = (SCREEN_SCREENPROP | SCREEN_COLORED | SCREEN_GRAPHICS),
  M_CEOL = (~(M_SPACE | C_WHICHCHAR)),
  M_MEND = (SCREEN_STANDOUT | SCREEN_UNDERLINE | SCREEN_BOLD | SCREEN_COLORED |
            SCREEN_GRAPHICS),
};

#define SETPROP(var, prop) (var = (((var) & SCREEN_DIRTY) | prop))

typedef unsigned short l_prop;

struct ScreenLine {
  struct Utf8 *lineimage;
  l_prop *lineprop;
  short isdirty;
  short eol;
};

struct Screen {
  struct ScreenLine **ScreenImage;
  int CurLine;
  int CurColumn;
  int graph_enabled;
};

struct Screen *scr_get();
void scr_setup(int LINES, int COLS);
void scr_clear();
void scr_move(int line, int column);
void scr_touch_line();
void scr_touch_column(int);
void scr_wrap();
void scr_clrtoeol(); /* conflicts with curs_clear(3)? */
void scr_clrtoeolx();
void scr_clrtobot();
void scr_clrtobotx();
bool scr_need_redraw(struct Utf8 c1, l_prop pr1, struct Utf8 c2, l_prop pr2);
void scr_addutf8(const uint8_t *utf8);
void scr_addch(char c);
void scr_addstr(const char *s);
void scr_addnstr(const char *s, int n);
void scr_addnstr_sup(const char *s, int n);
void scr_standout();
void scr_standend();
void scr_toggle_stand(void);
void scr_bold();
void scr_boldend();
void scr_underline();
void scr_underlineend();
void scr_graphend();
void scr_graphstart();
void scr_message(const char *s, int return_x, int return_y);
