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
  _size = size;

  if (LINES() + 1 > max_LINES) {
    max_LINES = LINES() + 1;
    max_COLS = 0;
    ScreenElem = (Screen *)New_N(Screen, max_LINES);
    ScreenImage = (Screen **)New_N(Screen *, max_LINES);
  }
  if (COLS() + 1 > max_COLS) {
    max_COLS = COLS() + 1;
    for (int i = 0; i < max_LINES; i++) {
      ScreenElem[i].lineimage = (Utf8 *)New_N(Utf8, max_COLS);
      ScreenElem[i].lineprop = (l_prop *)New_N(l_prop, max_COLS);
    }
  }

  int i;
  for (i = 0; i < LINES(); i++) {
    ScreenImage[i] = &ScreenElem[i];
    ScreenImage[i]->lineprop[0] = S_EOL;
    ScreenImage[i]->isdirty = {};
  }
  for (; i < max_LINES; i++) {
    ScreenElem[i].isdirty = L_UNUSED;
  }

  clear();
}

void TermScreen::clear(void) {
  // term_writestr(_entry.T_cl);

  int i, j;
  l_prop *p;
  move(0, 0);
  for (i = 0; i < LINES(); i++) {
    ScreenImage[i]->isdirty = {};
    p = ScreenImage[i]->lineprop;
    for (j = 0; j < COLS(); j++) {
      p[j] = S_EOL;
    }
  }
  CurrentMode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void TermScreen::clrtoeol() { /* Clear to the end of line */
  int i;
  auto lprop = ScreenImage[CurLine]->lineprop;

  if (lprop[CurColumn] & S_EOL)
    return;

  if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) ||
      ScreenImage[CurLine]->eol > CurColumn)
    ScreenImage[CurLine]->eol = CurColumn;

  ScreenImage[CurLine]->isdirty =
      (LineDirtyFlags)(ScreenImage[CurLine]->isdirty | L_CLRTOEOL);
  touch_line();
  for (i = CurColumn; i < COLS() && !(lprop[i] & S_EOL); i++) {
    lprop[i] = S_EOL | S_DIRTY;
  }
}

void TermScreen::clrtoeolx(void) { clrtoeol(); }

void TermScreen::clrtobot_eol(const std::function<void()> &ffclrtoeol) {
  int l = CurLine;
  int c = CurColumn;
  clrtoeol();
  CurColumn = 0;
  CurLine++;
  for (; CurLine < LINES(); CurLine++)
    clrtoeol();
  CurLine = l;
  CurColumn = c;
}

void TermScreen::addstr(const char *s) {
  while (*s != '\0')
    addch(*(s++));
}

void TermScreen::addnstr(const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    addch(*(s++));
}

void TermScreen::addnstr_sup(const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    addch(*(s++));
  for (; i < n; i++)
    addch(' ');
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

const Utf8 SPACE = {' ', 0, 0, 0};
#define M_SPACE (S_SCREENPROP | S_COLORED | S_GRAPHICS)

static int need_redraw(const Utf8 &c1, l_prop pr1, const Utf8 &c2, l_prop pr2) {
  if (c1 != c2)
    return 1;
  if (c1 == Utf8{' '})
    return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

  if ((pr1 ^ pr2) & ~S_DIRTY)
    return 1;

  return 0;
}

static void cursor(const RowCol p) {
  std::cout << "\e[" << p.row << ";" << p.col << "H";
}

void TermScreen::print() {
  // using namespace ftxui;
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(COLS()),
                                      ftxui::Dimension::Fixed(LINES()));

  for (int y = 0; y <= LASTLINE(); y++) {
    auto pc = ScreenImage[y]->lineimage;
    auto pr = ScreenImage[y]->lineprop;
    for (int x = 0; x < COLS(); x++) {
      if (CHMODE(pr[x]) == C_WCHAR2) {
      } else {
        auto &pixel = screen.PixelAt(x, y);
        auto &utf8 = pc[x];
        if (utf8.view().size() == 0) {
          pixel.character = " ";
        } else {
          pixel.character = pc[x].view();
        }

        auto p = pr[x];
        pixel.inverted = p & S_STANDOUT;
        pixel.underlined = p & S_UNDERLINE;
        pixel.bold = p & S_BOLD;
      }
    }
  }

  std::cout << "\e[?25l";
  std::flush(std::cout);
  cursor({
      .row = 1,
      .col = 1,
  });
  // screen.SetCursor({
  //     .x = CurColumn + 1,
  //     .y = CurLine + 1,
  //     .shape = ftxui::Screen::Cursor::Shape::Hidden,
  // });
  screen.Print();
  // std::cout << "\e[2 q";
  cursor({
      .row = CurLine + 1,
      .col = CurColumn + 1,
  });
  std::cout << "\e[?25h";
  std::flush(std::cout);
}

void TermScreen::move(int line, int column) {
  if (line >= 0 && line < LINES())
    CurLine = line;
  if (column >= 0 && column < COLS())
    CurColumn = column;
}

void TermScreen::toggle_stand(void) {
  l_prop *pr = ScreenImage[CurLine]->lineprop;
  pr[CurColumn] ^= S_STANDOUT;
}

// void setfcolor(int color) {
//   CurrentMode &= ~COL_FCOLOR;
//   if ((color & 0xf) <= 7)
//     CurrentMode |= (((color & 7) | 8) << 8);
// }

void TermScreen::wrap(void) {
  if (CurLine == LASTLINE()) {
    return;
  }
  CurLine++;
  CurColumn = 0;
}

void TermScreen::addmch(const Utf8 &utf8) {
  if (CurColumn == COLS())
    wrap();
  if (CurColumn >= COLS())
    return;
  auto p = ScreenImage[CurLine]->lineimage;
  auto pr = ScreenImage[CurLine]->lineprop;

  if (pr[CurColumn] & S_EOL) {
    if (utf8 == SPACE && !(CurrentMode & M_SPACE)) {
      CurColumn++;
      return;
    }
    for (int i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
      SETCH(p[i], SPACE, 1);
      SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
    }
  }

  auto c = utf8.b0;
  if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
    SETCHMODE(CurrentMode, C_CTRL);
  else if (utf8.view().size() > 1)
    SETCHMODE(CurrentMode, C_WCHAR1);
  else if (!IS_CNTRL(c))
    SETCHMODE(CurrentMode, C_ASCII);
  else
    return;

  int width = utf8.cols();
  /* Required to erase bold or underlined character for some * terminal
   * emulators. */
  int i = CurColumn + width - 1;
  if (i < COLS() &&
      (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], utf8, CurrentMode)) ||
       ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
    touch_line();
    i++;
    if (i < COLS()) {
      touch_column(i);
      if (pr[i] & S_EOL) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
      } else {
        for (i++; i < COLS() && CHMODE(pr[i]) == C_WCHAR2; i++)
          touch_column(i);
      }
    }
  }

  if (CurColumn + width > COLS()) {
    touch_line();
    for (i = CurColumn; i < COLS(); i++) {
      SETCH(p[i], SPACE, 1);
      SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
      touch_column(i);
    }
    wrap();
    if (CurColumn + width > COLS())
      return;
    p = ScreenImage[CurLine]->lineimage;
    pr = ScreenImage[CurLine]->lineprop;
  }
  if (CHMODE(pr[CurColumn]) == C_WCHAR2) {
    touch_line();
    for (i = CurColumn - 1; i >= 0; i--) {
      l_prop l = CHMODE(pr[i]);
      SETCH(p[i], SPACE, 1);
      SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
      touch_column(i);
      if (l != C_WCHAR2)
        break;
    }
  }
  if (CHMODE(CurrentMode) != C_CTRL) {
    if (need_redraw(p[CurColumn], pr[CurColumn], utf8, CurrentMode)) {
      SETCH(p[CurColumn], utf8, len);
      SETPROP(pr[CurColumn], CurrentMode);
      touch_line();
      touch_column(CurColumn);
      SETCHMODE(CurrentMode, C_WCHAR2);
      for (i = CurColumn + 1; i < CurColumn + width; i++) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
        touch_column(i);
      }
      for (; i < COLS() && CHMODE(pr[i]) == C_WCHAR2; i++) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
        touch_column(i);
      }
    }
    CurColumn += width;
  } else if (c == '\t') {
    auto dest = (CurColumn + tab_step) / tab_step * tab_step;
    if (dest >= COLS()) {
      wrap();
      touch_line();
      dest = tab_step;
      p = ScreenImage[CurLine]->lineimage;
      pr = ScreenImage[CurLine]->lineprop;
    }
    for (i = CurColumn; i < dest; i++) {
      if (need_redraw(p[i], pr[i], SPACE, CurrentMode)) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], CurrentMode);
        touch_line();
        touch_column(i);
      }
    }
    CurColumn = i;
  } else if (c == '\n') {
    wrap();
  } else if (c == '\r') { /* Carriage return */
    CurColumn = 0;
  } else if (c == '\b' && CurColumn > 0) { /* Backspace */
    CurColumn--;
    while (CurColumn > 0 && CHMODE(pr[CurColumn]) == C_WCHAR2)
      CurColumn--;
  }
}
