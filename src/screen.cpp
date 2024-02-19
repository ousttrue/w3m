#include "screen.h"
#include "alloc.h"
#include "myctype.h"
#include <ftxui/screen/screen.hpp>
#include <iostream>

#define CHMODE(c) ((c) & C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define SETCH(var, ch, len) ((var) = (ch))
#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

void TermScreen::setupscreen(const RowCol &size) {
  _screen = std::make_shared<ftxui::Screen>(size.col, size.row);
}

void TermScreen::clear(void) {
  // term_writestr(_entry.T_cl);
  _screen->Clear();
  CurrentMode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void TermScreen::clrtoeol(const RowCol &pos) { /* Clear to the end of line */
  if (pos.row >= 0 && pos.row < LINES()) {
    int y = pos.row + 1;
    for (int x = pos.col + 1; x <= COLS(); ++x) {
      _screen->PixelAt(x, y) = {};
    }
  }
}

void TermScreen::clrtobot_eol(
    const RowCol &pos, const std::function<void(const RowCol &pos)> &callback) {
  clrtoeol(pos);
  int CurLine = pos.row + 1;
  for (; CurLine < LINES(); CurLine++) {
    callback({.row = CurLine, .col = 0});
  }
}

RowCol TermScreen::addstr(RowCol pos, const char *s) {
  while (*s) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
  }
  return pos;
}

RowCol TermScreen::addnstr(RowCol pos, const char *s, int n) {
  for (int i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
    i += utf8.view().size();
  }
  return pos;
}

RowCol TermScreen::addnstr_sup(RowCol pos, const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
    i += utf8.view().size();
  }
  for (; i < n; i++) {
    addch(pos, ' ');
    pos.col += 1;
  }
  return pos;
}

#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_GRAPHICS)

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

static void cursor(const RowCol p) {
  std::cout << "\e[" << p.row << ";" << p.col << "H";
}

void TermScreen::print() {
  // using namespace ftxui;

  std::cout << "\e[?25l";
  std::flush(std::cout);
  ::cursor({
      .row = 1,
      .col = 1,
  });
  // screen.SetCursor({
  //     .x = CurColumn + 1,
  //     .y = CurLine + 1,
  //     .shape = ftxui::Screen::Cursor::Shape::Hidden,
  // });
  _screen->Print();
}

void TermScreen::cursor(const RowCol &pos) {
  // std::cout << "\e[2 q";
  ::cursor({
      .row = pos.row + 1,
      .col = pos.col + 1,
  });
  std::cout << "\e[?25h";
  std::flush(std::cout);
}

void TermScreen::toggle_stand(const RowCol &pos) {
  auto &pixel = _screen->PixelAt(pos.col, pos.row);
  pixel.inverted = !pixel.inverted;
}

void TermScreen::addmch(const RowCol &pos, const Utf8 &utf8) {
  auto &pixel = _screen->PixelAt(pos.col, pos.row);
  pixel.character = utf8.view();
}
