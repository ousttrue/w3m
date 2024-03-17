#pragma once
#include "rowcol.h"
#include "line_data.h"
#include "http_request.h"
#include "url.h"
#include "utf8.h"
#include "enum_template.h"
#include <ftxui/screen/terminal.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <optional>
#include <functional>
#include <memory>

extern bool FoldTextarea;

struct HtmlTag;

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

struct LineLayout : public ftxui::Node {
  LineData data;

  LineLayout();
  // LineLayout(int width);

  //
  // lines
  //
  Line *firstLine() { return data.firstLine(); }
  Line *lastLine() { return data.lastLine(); }
  const Line *lastLine() const { return data.lastLine(); }
  bool empty() const { return data.lines.empty(); }
  int linenumber(const Line *t) const { return data.linenumber(t); }
  bool isNull(Line *l) const { return linenumber(l) < 0; }
  bool hasNext(const Line *l) const { return linenumber(++l) >= 0; }
  bool hasPrev(const Line *l) const { return linenumber(--l) >= 0; }

  // short COLS = 0;
  // short LINES = 0;
  // // COLS/LINES と width height の違いは？
  // short width = 0;
  // short height = 0;

  // int pos = 0;
  // int visualpos = 0;
  // scroll position
  // int _topLine = 0;
private:
  RowCol _scroll = {0, 0};

public:
  RowCol scroll() const { return _scroll; }
  void scrollMoveRow(int row) { _scroll.row += row; }
  void scrollMoveCol(int col) { _scroll.col += col; }
  // leftPos
  // int scrollPos() const;
  Line *topLine() {
    //
    return &data.lines[_scroll.row];
  }
  // {
  //   if (_topLine < 0 || _topLine >= (int)data.lines.size()) {
  //     return nullptr;
  //   }
  //   return &data.lines[_topLine];
  // }

  // cursor position
private:
  RowCol _cursor = {0, 0};

public:
  RowCol cursor() const { return _cursor; }
  void cursorCol(int col) { _cursor.col = col; }
  int cursorPos() const {
    if (_cursor.row < data.lines.size()) {
      auto l = data.lines[_cursor.row];
      return l.columnPos(_cursor.col);
    }
    return _cursor.col;
  }
  void cursorPos(int pos) {
    auto l = data.lines[_cursor.row];
    _cursor.col = l.bytePosToColumn(pos);
  }
  void cursorMoveCol(int col) { _cursor.col += col; }
  void cursorRow(int row) { _cursor.row = row; }
  void cursorMoveRow(int row) { _cursor.row += row; }
  // int currentColumn = 0;
  Line *currentLine() {
    if (_cursor.row < data.lines.size()) {
      return &data.lines[_cursor.row];
    }
    return {};
  }
  // {
  //   auto _currentLine = _topLine + cursor.row;
  //   if (_currentLine < 0 || _currentLine >= (int)data.lines.size()) {
  //     return nullptr;
  //   }
  //   return &data.lines[_currentLine];
  // }

  const Line *currentLine() const {
    //
    return &data.lines[_cursor.row];
  }
  // {
  //   auto _currentLine = _topLine + cursor.row;
  //   if (_currentLine < 0 || _currentLine >= (int)data.lines.size()) {
  //     return nullptr;
  //   }
  //   return &data.lines[_currentLine];
  // }

  bool check_url = false;
  void chkURLBuffer();
  void reshape(int width);

  void clearBuffer();

  // void cursorUpDown(int n);
  // void gotoLine(int n);
  // void cursorUp0(int n);
  // void cursorUp(int n);
  // void cursorDown0(int n);
  // void cursorDown(int n);

  // int columnSkip(int offset);
  // void cursorRight(int n);
  // void cursorLeft(int n);
  void cursorHome();
  // void cursorXY(int x, int y);
  void restorePosition(const std::shared_ptr<LineLayout> &orig);
  void nscroll(int n);

  //
  // viewport
  //

  void nextY(int d, int n);
  void nextX(int d, int dy, int n);
  void _prevA(std::optional<Url> baseUrl, int n);
  void _nextA(std::optional<Url> baseUrl, int n);
  // void _movL(int n, int m);
  // void _movD(int n, int m);
  // void _movU(int n, int m);
  // void _movR(int n, int m);
  int prev_nonnull_line(Line *line);
  int next_nonnull_line(Line *line);
  void _goLine(const char *l, int prec_num);
  void shiftvisualpos(int shift);
  std::string getCurWord(int *spos, int *epos) const;
  std::string getCurWord() const {
    int s, e;
    return getCurWord(&s, &e);
  }

  Anchor *retrieveCurrentAnchor();
  Anchor *retrieveCurrentImg();
  FormAnchor *retrieveCurrentForm();
  void formResetBuffer(const AnchorList<FormAnchor> *formitem);
  void formUpdateBuffer(FormAnchor *a);

  std::shared_ptr<ftxui::Screen> _screen;
  int max_LINES = 0;
  int max_COLS = 0;
  int graph_enabled = 0;
  int tab_step = 8;
  ScreenFlags CurrentMode = {};

  // int cline = -1;
  // int ccolumn = -1;

public:
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
  RowCol root() const {
    return {
        .row = box_.y_min,
        .col = box_.x_min,
    };
  }
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
                 std::bind(&LineLayout::clrtoeol, this, std::placeholders::_1));
  }
  void clrtobotx(const RowCol &pos) {
    clrtobot_eol(
        pos, std::bind(&LineLayout::clrtoeolx, this, std::placeholders::_1));
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

  // std::string str(const RowCol &root, LineLayout *layout);
};

inline void nextChar(int &s, const Line *l) { s++; }
inline void prevChar(int &s, const Line *l) { s--; }
