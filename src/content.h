#pragma once
#include "utf8.h"
#include "rowcol.h"
#include "enum_template.h"
#include "line_layout.h"
#include <ftxui/screen/terminal.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <memory>

struct Utf8;

// Screen properties
enum class ScreenFlags : unsigned short {
  NORMAL = 0x00,
  STANDOUT = 0x01,
  UNDERLINE = 0x02,
  BOLD = 0x04,
  EOL = 0x08,
  SCREENPROP = 0x0f,

  GRAPHICS = 0x10,
  DIRTY = 0x20,

  /* Sort of Character */
  ASCII = 0x00,
  WCHAR1 = 0x40,
  WCHAR2 = 0x80,
  WHICHCHAR = 0xc0,
  CTRL = 0xc0,

  COLORED = 0xf00,
};
ENUM_OP_INSTANCE(ScreenFlags);

/* Line status */
enum LineDirtyFlags : unsigned short {
  L_DIRTY = 0x01,
  L_UNUSED = 0x02,
  L_NEED_CE = 0x04,
  L_CLRTOEOL = 0x08,
};

struct LineLayout;
class Content : public ftxui::Node {
  std::shared_ptr<ftxui::Screen> _screen;
  int max_LINES = 0;
  int max_COLS = 0;
  int graph_enabled = 0;
  int tab_step = 8;
  ScreenFlags CurrentMode = {};

  int cline = -1;
  int ccolumn = -1;

public:
  LineLayout *layout;

  void Render(ftxui::Screen &screen) override;

public:
  int ulmode = 0;
  int somode = 0;
  int bomode = 0;
  int anch_mode = 0;
  int emph_mode = 0;
  int imag_mode = 0;
  int form_mode = 0;
  int active_mode = 0;
  int visited_mode = 0;
  int mark_mode = 0;
  int graph_mode = 0;

  ftxui::Pixel &pixel(const RowCol &pos) {
    return _screen->PixelAt(pos.col, pos.row);
  }

public:
  int COLS() const { return _screen->dimx(); }
  int LINES() const { return _screen->dimy(); }
  int LASTLINE() const { return (LINES() - 1); }
  void setupscreen(const RowCol &size);
  void clear();
  void clrtoeol(const RowCol &pos);
  void clrtoeolx(const RowCol &pos) { clrtoeol(pos); }
  void clrtobot_eol(const RowCol &pos,
                    const std::function<void(const RowCol &)> &);
  void clrtobot(const RowCol &pos) {
    clrtobot_eol(pos,
                 std::bind(&Content::clrtoeol, this, std::placeholders::_1));
  }
  void clrtobotx(const RowCol &pos) {
    clrtobot_eol(pos,
                 std::bind(&Content::clrtoeolx, this, std::placeholders::_1));
  }
  void no_clrtoeol();
  void addmch(const RowCol &pos, const Utf8 &utf8, ScreenFlags mode = {});
  void addch(const RowCol &pos, char c) { addmch(pos, {(char8_t)c, 0, 0, 0}); }
  RowCol addstr(RowCol pos, const char *s);
  RowCol addnstr(RowCol pos, const char *s, int n, ScreenFlags mode = {});
  RowCol addnstr_sup(RowCol pos, const char *s, int n);
  void toggle_stand(const RowCol &pos);
  void bold(void) { CurrentMode |= ScreenFlags::BOLD; }
  void boldend(void) { CurrentMode &= ~ScreenFlags::BOLD; }
  void underline(void) { CurrentMode |= ScreenFlags::UNDERLINE; }
  void underlineend(void) { CurrentMode &= ~ScreenFlags::UNDERLINE; }
  void graphstart(void) { CurrentMode |= ScreenFlags::GRAPHICS; }
  void graphend(void) { CurrentMode &= ~ScreenFlags::GRAPHICS; }
  void standout(void) { CurrentMode |= ScreenFlags::STANDOUT; }
  void standend(void) { CurrentMode &= ~ScreenFlags::STANDOUT; }

  RowCol redrawLine(RowCol pixel, int l, int cols, int scrollLeft);
  // int redrawLineRegion(const RowCol &root, LineLayout *layout, Line *l, int
  // i,
  //                      int bpos, int epos);

  std::string str(const RowCol &root, LineLayout *layout);
};
