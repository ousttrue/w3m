#pragma once
#include "termentry.h"
#include "utf8.h"
#include <stdio.h>
#include <functional>

struct Utf8;
using l_prop = unsigned short;

// Screen properties
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

struct RowCol {
  int row;
  int col;
};

class TermScreen {
  RowCol _size;
  int COLS() const { return _size.col; }
  int LINES() const { return _size.row; }

  TermEntry _entry;

  int max_LINES = 0;
  int max_COLS = 0;
  int graph_enabled = 0;
  int tab_step = 8;
  Screen *ScreenElem = {};
  Screen **ScreenImage = {};
  l_prop CurrentMode = {};
  int CurLine;
  int CurColumn;

  void touch_column(int col) {
    if (col >= 0 && col < COLS())
      ScreenImage[CurLine]->lineprop[col] |= S_DIRTY;
  }

  void touch_line(void) {
    if (!(ScreenImage[CurLine]->isdirty & L_DIRTY)) {
      int i;
      for (i = 0; i < COLS(); i++)
        ScreenImage[CurLine]->lineprop[i] &= ~S_DIRTY;
      ScreenImage[CurLine]->isdirty =
          (LineDirtyFlags)(ScreenImage[CurLine]->isdirty | L_DIRTY);
    }
  }

public:
  void setupscreen(const RowCol &size, const TermEntry &entry);
  void clear();
  void _refresh(FILE *ttyf);
  void clrtoeol();
  void refresh(FILE *ttyf);
  void clrtoeolx();
  void clrtobot_eol(const std::function<void()> &);
  void clrtobot(void) { clrtobot_eol(std::bind(&TermScreen::clrtoeol, this)); }
  void clrtobotx(void) {
    clrtobot_eol(std::bind(&TermScreen::clrtoeolx, this));
  }
  void wrap();
  void no_clrtoeol();
  void move(int line, int column);
  void addmch(const Utf8 &utf8);
  void addch(char c) { addmch({(char8_t)c, 0, 0, 0}); }
  void addstr(const char *s);
  void addnstr(const char *s, int n);
  void addnstr_sup(const char *s, int n);
  void toggle_stand();
  void bold(void) { CurrentMode |= S_BOLD; }
  void boldend(void) { CurrentMode &= ~S_BOLD; }
  void underline(void) { CurrentMode |= S_UNDERLINE; }
  void underlineend(void) { CurrentMode &= ~S_UNDERLINE; }
  void graphstart(void) { CurrentMode |= S_GRAPHICS; }
  void graphend(void) { CurrentMode &= ~S_GRAPHICS; }
  void standout(void) { CurrentMode |= S_STANDOUT; }
  void standend(void) { CurrentMode &= ~S_STANDOUT; }
};
