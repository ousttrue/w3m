#pragma once

#define SPACE ' '

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
  S_NORMAL = 0x00,
  S_STANDOUT = 0x01,
  S_UNDERLINE = 0x02,
  S_BOLD = 0x04,
  S_EOL = 0x08,
  S_SCREENPROP = 0x0f,

  S_GRAPHICS = 0x10,
  S_DIRTY = 0x20,
  C_ASCII = 0x00,
  C_WHICHCHAR = 0xc0,
  C_CTRL = 0xc0,

  S_COLORED = 0xf00,

  M_SPACE = (S_SCREENPROP | S_COLORED | S_GRAPHICS),
  M_CEOL = (~(M_SPACE | C_WHICHCHAR)),
  M_MEND = (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_GRAPHICS),
};

#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))

typedef unsigned short l_prop;

typedef struct scline {
  char *lineimage;
  l_prop *lineprop;
  short isdirty;
  short eol;
} Screen;

typedef struct Scr {
  Screen **ScreenImage;
  int CurLine;
  int CurColumn;
  int graph_enabled;
} Scr;

Scr *scr_get();
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
int scr_need_redraw(char c1, l_prop pr1, char c2, l_prop pr2);
void scr_addch(char c);
void scr_addstr(char *s);
void scr_addnstr(char *s, int n);
void scr_addnstr_sup(char *s, int n);
void scr_standout();
void scr_standend();
void scr_toggle_stand(void);
void scr_bold();
void scr_boldend();
void scr_underline();
void scr_underlineend();
void scr_graphend();
void scr_graphstart();
