#pragma once
#include <stdio.h>

struct Utf8;
using l_prop = unsigned short;

/* Screen properties */
enum ScreenFlags : unsigned short {
  S_NORMAL = 0x00,
  S_STANDOUT = 0x01,
  S_UNDERLINE = 0x02,
  S_BOLD = 0x04,
  S_EOL = 0x08,
  S_SCREENPROP = 0x0f,

  S_GRAPHICS = 0x10,
  S_DIRTY = 0x20,

  /* Sort of Character */
  C_ASCII = 0x00,
  C_WCHAR1 = 0x40,
  C_WCHAR2 = 0x80,
  C_WHICHCHAR = 0xc0,
  C_CTRL = 0xc0,

  S_COLORED = 0xf00,
};

/* Line status */
enum LineDirtyFlags : unsigned short {
  L_DIRTY = 0x01,
  L_UNUSED = 0x02,
  L_NEED_CE = 0x04,
  L_CLRTOEOL = 0x08,
};

struct Screen {
  Utf8 *lineimage;
  l_prop *lineprop;
  LineDirtyFlags isdirty;
  short eol;
};

struct TermEntry;
void setupscreen(const TermEntry &entry);
void refresh(FILE *ttyf);
void clear();
void clrtoeol();
void clrtoeolx();
void clrtobot();
void clrtobotx();
void no_clrtoeol();
void move(int line, int column);
void addch(char c);
void addmch(const Utf8 &utf8);
void addstr(char *s);
void addnstr(char *s, int n);
void addnstr_sup(char *s, int n);
void standout();
void standend();
void toggle_stand();
void bold();
void boldend();
void underline();
void underlineend();
void graphstart();
void graphend();
