#pragma once
#include "utf8.h"
#include "rowcol.h"
#include "enum_template.h"
#include <ftxui/screen/screen.hpp>
#include <functional>
#include <memory>

extern bool displayLink;
extern int displayLineInfo;

struct Utf8;
using l_prop = unsigned short;

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

struct TermEntry;
struct Buffer;
struct TabBuffer;
struct LineLayout;
struct AnchorList;
class Screen {
  std::shared_ptr<ftxui::Screen> _screen;
  int max_LINES = 0;
  int max_COLS = 0;
  int graph_enabled = 0;
  int tab_step = 8;
  ScreenFlags CurrentMode = {};

  Line *cline = NULL;
  int ccolumn = -1;
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
  std::shared_ptr<Buffer> save_current_buf;

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
  void print();
  void clrtoeolx(const RowCol &pos) { clrtoeol(pos); }
  void clrtobot_eol(const RowCol &pos,
                    const std::function<void(const RowCol &)> &);
  void clrtobot(const RowCol &pos) {
    clrtobot_eol(pos,
                 std::bind(&Screen::clrtoeol, this, std::placeholders::_1));
  }
  void clrtobotx(const RowCol &pos) {
    clrtobot_eol(pos,
                 std::bind(&Screen::clrtoeolx, this, std::placeholders::_1));
  }
  void no_clrtoeol();
  void addmch(const RowCol &pos, const Utf8 &utf8);
  void addch(const RowCol &pos, char c) { addmch(pos, {(char8_t)c, 0, 0, 0}); }
  RowCol addstr(RowCol pos, const char *s);
  RowCol addnstr(RowCol pos, const char *s, int n);
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
  void cursor(const RowCol &pos);
  int redrawLineRegion(const std::shared_ptr<Buffer> &buf, Line *l, int i,
                       int bpos, int epos);
  Line *redrawLine(LineLayout *buf, Line *l, int i);
  void redrawNLine(const std::shared_ptr<Buffer> &buf, int n);
  void drawAnchorCursor0(const std::shared_ptr<Buffer> &buf, AnchorList *al,
                         int hseq, int prevhseq, int tline, int eline,
                         int active);
  void drawAnchorCursor(const std::shared_ptr<Buffer> &buf);
  Str *make_lastline_link(const std::shared_ptr<Buffer> &buf, const char *title,
                          const char *url);
  Str *make_lastline_message(const std::shared_ptr<Buffer> &buf);
  void display(int ny, int width, TabBuffer *currentTab);
};
