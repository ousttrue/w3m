#include "line_layout.h"
#include "quote.h"
#include "myctype.h"
#include "url.h"
#include "utf8.h"
#include <sstream>

LineLayout::LineLayout() : formated(new LineData) {}

std::string LineLayout::row_status() const {
  std::stringstream ss;
  ss << "row:" << visual.cursor().row;
  return ss.str();
}

std::string LineLayout::col_status() const {

  std::stringstream ss;
  ss << "col:" << visual.cursor().col;
  return ss.str();
}

/* go to the next downward/upward anchor */
void LineLayout::nextY(int d, int n) {
  if (empty()) {
    return;
  }

  auto hl = this->formated->_hmarklist;
  if (hl->size() == 0) {
    return;
  }

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = this->retrieveCurrentForm();
  }

  int x = this->cursorPos();
  int y = this->visual.cursor().row + d;
  Anchor *pan = nullptr;
  int hseq = -1;
  for (int i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = nullptr;
    for (; y >= 0 && y <= linenumber(this->lastLine()); y += d) {
      if (this->formated->_href) {
        an = this->formated->_href->retrieveAnchor({.line = y, .pos = x});
      }
      if (!an && this->formated->_formitem) {
        an = this->formated->_formitem->retrieveAnchor({.line = y, .pos = x});
      }
      if (an && hseq != abs(an->hseq)) {
        pan = an;
        break;
      }
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  this->visual.cursorRow(pan->start.line);
}

/* go to the next left/right anchor */
void LineLayout::nextX(int d, int dy, int n) {
  if (empty())
    return;

  auto hl = this->formated->_hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = retrieveCurrentForm();
  }

  auto l = this->currentLine();
  auto x = this->cursorPos();
  auto y = linenumber(l);
  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = nullptr;
    while (1) {
      for (; x >= 0 && x < l->len(); x += d) {
        if (this->formated->_href) {
          an = this->formated->_href->retrieveAnchor({.line = y, .pos = x});
        }
        if (!an && this->formated->_formitem) {
          an = this->formated->_formitem->retrieveAnchor({.line = y, .pos = x});
        }
        if (an) {
          pan = an;
          break;
        }
      }
      if (!dy || an)
        break;
      (dy > 0) ? ++l : --l;
      if (!l)
        break;
      x = (d > 0) ? 0 : l->len() - 1;
      y = linenumber(l);
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  this->visual.cursorRow(y);
  this->cursorPos(pan->start.pos);
}

/* go to the previous anchor */
void LineLayout::_prevA(std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->formated->_hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  int y = this->visual.cursor().row;
  int x = this->cursorPos();

  for (int i = 0; i < n; i++) {
    auto pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq - 1;
      do {
        if (hseq < 0) {
          an = pan;
          goto _end;
        }
        auto po = hl->get(hseq);
        if (this->formated->_href) {
          an = this->formated->_href->retrieveAnchor(*po);
        }
        if (an == nullptr && this->formated->_formitem) {
          an = this->formated->_formitem->retrieveAnchor(*po);
        }
        hseq--;
      } while (an == nullptr || an == pan);
    } else {
      an = this->formated->_href->closest_prev_anchor((Anchor *)nullptr, x, y);
      an = this->formated->_formitem->closest_prev_anchor(an, x, y);
      if (an == nullptr) {
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
    }
  }

_end:
  if (an == nullptr || an->hseq < 0)
    return;

  {
    auto po = hl->get(an->hseq);
    this->visual.cursorRow(po->line);
    this->cursorPos(po->pos);
  }
}

/* go to the next anchor */
void LineLayout::_nextA(std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->formated->_hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  auto y = visual.cursor().row;
  auto x = this->cursorPos();

  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq + 1;
      do {
        if (hseq >= (int)hl->size()) {
          an = pan;
          goto _end;
        }
        auto po = hl->get(hseq);
        if (this->formated->_href) {
          an = this->formated->_href->retrieveAnchor(*po);
        }
        if (an == nullptr && this->formated->_formitem) {
          an = this->formated->_formitem->retrieveAnchor(*po);
        }
        hseq++;
      } while (an == nullptr || an == pan);
    } else {
      an = this->formated->_href->closest_next_anchor((Anchor *)nullptr, x, y);
      an = this->formated->_formitem->closest_next_anchor(an, x, y);
      if (an == nullptr) {
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
    }
  }

_end:
  if (an == nullptr || an->hseq < 0)
    return;

  auto po = hl->get(an->hseq);
  this->visual.cursorRow(po->line);
  this->cursorPos(po->pos);
}

int LineLayout::prev_nonnull_line(Line *line) {
  Line *l;
  for (l = line; !isNull(l) && l->len() == 0; --l)
    ;
  if (l == nullptr || l->len() == 0)
    return -1;

  this->visual.cursorRow(linenumber(l));
  if (l != line)
    this->cursorPos(this->currentLine()->len());
  return 0;
}

int LineLayout::next_nonnull_line(Line *line) {
  Line *l;
  for (l = line; !isNull(l) && l->len() == 0; ++l)
    ;
  if (l == nullptr || l->len() == 0)
    return -1;

  this->visual.cursorRow(linenumber(l));
  if (l != line)
    this->visual.cursorCol(0);
  return 0;
}

/* Go to specified line */
void LineLayout::_goLine(std::string_view l) {
  if (l.empty() || this->currentLine() == nullptr) {
    return;
  }

  this->visual.cursorCol(0);
  if (l[0] == '^') {
    this->visual.cursorHome();
  } else if (l[0] == '$') {
    // this->_scroll.row = linenumber(this->lastLine()) - (this->LINES + 1) / 2;
    this->visual.cursorRow(linenumber(this->lastLine()));
  } else {
    this->visual.cursorRow(stoi(l));
  }
}

std::string LineLayout::getCurWord(int *spos, int *epos) const {
  auto l = this->currentLine();
  if (l == nullptr)
    return {};

  *spos = 0;
  *epos = 0;
  auto p = l->lineBuf();
  auto e = this->cursorPos();
  while (e > 0 && !is_wordchar(p[e]))
    prevChar(e, l);
  if (!is_wordchar(p[e]))
    return {};
  int b = e;
  while (b > 0) {
    int tmp = b;
    prevChar(tmp, l);
    if (!is_wordchar(p[tmp]))
      break;
    b = tmp;
  }
  while (e < l->len() && is_wordchar(p[e])) {
    e++;
  }
  *spos = b;
  *epos = e;
  return {p + b, p + e};
}

Anchor *LineLayout::retrieveCurrentAnchor() {
  if (!this->currentLine() || !this->formated->_href)
    return NULL;
  return this->formated->_href->retrieveAnchor(
      {.line = visual.cursor().row, .pos = this->cursorPos()});
}

Anchor *LineLayout::retrieveCurrentImg() {
  if (!this->currentLine() || !this->formated->_img)
    return NULL;
  return this->formated->_img->retrieveAnchor(
      {.line = visual.cursor().row, .pos = this->cursorPos()});
}

FormAnchor *LineLayout::retrieveCurrentForm() {
  if (!this->currentLine() || !this->formated->_formitem)
    return NULL;
  return this->formated->_formitem->retrieveAnchor(
      {.line = visual.cursor().row, .pos = this->cursorPos()});
}

void LineLayout::formResetBuffer(const AnchorList<FormAnchor> *formitem) {

  if (this->formated->_formitem == NULL || formitem == NULL)
    return;

  for (size_t i = 0;
       i < this->formated->_formitem->size() && i < formitem->size(); i++) {
    auto a = &this->formated->_formitem->anchors[i];
    if (a->y != a->start.line)
      continue;

    auto f1 = a->formItem;
    auto f2 = formitem->anchors[i].formItem;
    if (f1->type != f2->type || f1->name != f2->name)
      break; /* What's happening */
    switch (f1->type) {
    case FORM_INPUT_TEXT:
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_FILE:
    case FORM_TEXTAREA:
      f1->value = f2->value;
      f1->init_value = f2->init_value;
      break;
    case FORM_INPUT_CHECKBOX:
    case FORM_INPUT_RADIO:
      f1->checked = f2->checked;
      f1->init_checked = f2->init_checked;
      break;
    case FORM_SELECT:
      break;
    default:
      continue;
    }

    this->formUpdateBuffer(a);
  }
}

void LineLayout::formUpdateBuffer(FormAnchor *a) {

  int spos, epos;
  CursorAndScroll backup = this->visual;
  this->visual.cursorRow(a->start.line);
  auto item = a->formItem;
  if (!item) {
    return;
  }
  switch (item->type) {
  case FORM_TEXTAREA:
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_PASSWORD:
  case FORM_INPUT_CHECKBOX:
  case FORM_INPUT_RADIO:
    spos = a->start.pos;
    epos = a->end.pos;
    break;
  default:
    spos = a->start.pos + 1;
    epos = a->end.pos - 1;
  }

  // char *p;
  // int rows, c_rows, pos, col = 0;
  // Line *l;
  switch (item->type) {
  case FORM_INPUT_CHECKBOX:
  case FORM_INPUT_RADIO:
    if (this->currentLine() == NULL || spos >= this->currentLine()->len() ||
        spos < 0)
      break;
    if (item->checked)
      this->currentLine()->lineBuf(spos, '*');
    else
      this->currentLine()->lineBuf(spos, ' ');
    break;
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_PASSWORD:
  case FORM_TEXTAREA: {
    if (item->value.empty())
      break;
    std::string p = item->value;

    auto l = this->currentLine();
    if (!l)
      break;
    if (item->type == FORM_TEXTAREA) {
      int n = a->y - this->visual.cursor().row;
      if (n > 0)
        for (; !this->isNull(l) && n; --l, n--)
          ;
      else if (n < 0)
        for (; !this->isNull(l) && n; --l, n++)
          ;
      if (!l)
        break;
    }
    auto rows = item->rows ? item->rows : 1;
    auto col = l->bytePosToColumn(a->start.pos);
    for (int c_rows = 0; c_rows < rows; c_rows++, ++l) {
      if (this->isNull(l))
        break;
      if (rows > 1 && this->formated->_formitem) {
        int pos = l->columnPos(col);
        a = this->formated->_formitem->retrieveAnchor(
            {.line = this->linenumber(l), .pos = pos});
        if (a == NULL)
          break;
        spos = a->start.pos;
        epos = a->end.pos;
      }
      if (a->start.line != a->end.line || spos > epos || epos >= l->len() ||
          spos < 0 || epos < 0 || l->bytePosToColumn(epos) < col)
        break;

      auto pos =
          l->form_update_line(p, spos, epos, l->bytePosToColumn(epos) - col,
                              rows > 1, item->type == FORM_INPUT_PASSWORD);
      if (pos != epos) {
        this->formated->_href->shiftAnchorPosition(
            this->formated->_hmarklist.get(), a->start.line, spos, pos - epos);
        this->formated->_name->shiftAnchorPosition(
            this->formated->_hmarklist.get(), a->start.line, spos, pos - epos);
        this->formated->_img->shiftAnchorPosition(
            this->formated->_hmarklist.get(), a->start.line, spos, pos - epos);
        this->formated->_formitem->shiftAnchorPosition(
            this->formated->_hmarklist.get(), a->start.line, spos, pos - epos);
      }
    }
    break;
  }

  default:
    break;
  }
  this->visual = backup;
}

void LineLayout::setupscreen(const RowCol &size) {
  _screen = std::make_shared<ftxui::Screen>(size.col, size.row);
  this->formated->clear(size.col);
}

void LineLayout::clear(void) {
  _screen->Clear();
  CurrentMode = ScreenFlags::ASCII;
}

void LineLayout::clrtoeol(const RowCol &pos) { /* Clear to the end of line */
  if (pos.row >= 0 && pos.row < LINES()) {
    int y = pos.row + 1;
    for (int x = pos.col + 1; x <= COLS(); ++x) {
      _screen->PixelAt(x, y) = {};
    }
  }
}

void LineLayout::clrtobot_eol(
    const RowCol &pos, const std::function<void(const RowCol &pos)> &callback) {
  clrtoeol(pos);
  int CurLine = pos.row + 1;
  for (; CurLine < LINES(); CurLine++) {
    callback({.row = CurLine, .col = 0});
  }
}

RowCol LineLayout::addstr(RowCol pos, const char *s) {
  while (*s) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
  }
  return pos;
}

RowCol LineLayout::addnstr(RowCol pos, const char *s, int n, ScreenFlags mode) {
  for (int i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8, mode);
    pos.col += utf8.width();
    s += utf8.view().size();
    i += utf8.view().size();
  }
  return pos;
}

RowCol LineLayout::addnstr_sup(RowCol pos, const char *s, int n) {
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

void LineLayout::toggle_stand(const RowCol &pos) {
  auto &pixel = this->pixel(pos);
  pixel.inverted = !pixel.inverted;
}

void LineLayout::addmch(const RowCol &pos, const Utf8 &utf8, ScreenFlags mode) {
  auto &pixel = this->pixel(pos);
  pixel.character = utf8.view();
  pixel.bold = (int)(mode & ScreenFlags::BOLD);
  pixel.underlined = (int)(mode & ScreenFlags::UNDERLINE);
}

void LineLayout::Render(ftxui::Screen &screen) {
  //
  // render to _screen
  //
  _screen = std::make_shared<ftxui::Screen>(box_.x_max - box_.x_min + 1,
                                            box_.y_max - box_.y_min + 1);
  auto l = this->visual.scroll().row;
  int i = 0;
  int height = box_.y_max - box_.y_min + 1;
  int width = box_.x_max - box_.x_min + 1;

  visual.view({.row = height, .col = width});

  for (; i < height; ++i, ++l) {
    auto pixel = this->redrawLine(
        {
            .row = i,
            .col = 0,
        },
        l, width, this->visual.scroll().col);
    this->clrtoeolx(pixel);
  }
  // clear remain
  this->clrtobotx({
      .row = i,
      .col = 0,
  });
  // cline = layout->_topLine;
  // ccolumn = layout->currentColumn;

  //
  // _screen to screen
  //
  int x = box_.x_min;
  const int y = box_.y_min;
  if (y > box_.y_max) {
    return;
  }
  // draw line
  for (int row = 0; row < screen.dimy(); ++row) {
    for (int col = 0; col < screen.dimx(); ++col) {
      auto p = _screen->PixelAt(col, row);
      screen.PixelAt(x + col, y + row) = p;
    }
  }

  auto cursor = visual.cursor();
  auto scroll = visual.scroll();
  screen.SetCursor(ftxui::Screen::Cursor{
      .x = x + cursor.col - scroll.col,
      .y = y + cursor.row - scroll.row,
      .shape = ftxui::Screen::Cursor::Shape::Block,
  });
}

static ScreenFlags propToFlag(Lineprop prop) {
  ScreenFlags flag =
      (prop & PE_BOLD ? ScreenFlags::BOLD : ScreenFlags::NORMAL) |
      (prop & PE_UNDER ? ScreenFlags::UNDERLINE : ScreenFlags::NORMAL) |
      (prop & PE_STAND ? ScreenFlags::STANDOUT : ScreenFlags::NORMAL);
  return flag;
}

RowCol LineLayout::redrawLine(RowCol pixel, int _l, int cols, int scrollLeft) {
  if (_l < 0 || _l >= (int)this->formated->lines.size()) {
    return pixel;
  }

  auto l = &this->formated->lines[_l];
  int pos = l->columnPos(pixel.col);
  auto p = l->lineBuf() + pos;
  auto pr = l->propBuf() + pos;
  int col = pixel.col; // l->bytePosToColumn(pos);

  if (col < scrollLeft) {
    // TODO:
    assert(false);
    // for (col = scrollLeft; col < nextCol; col++) {
    //   this->addch(pixel, ' ');
    //   ++pixel.col;
    // }
  }

  for (int j = 0; pos + j < l->len();) {
    int delta = get_mclen(&p[j]);
    int nextCol = l->bytePosToColumn(pos + j + delta);
    if (nextCol - scrollLeft > cols) {
      break;
    }

    if (p[j] == '\t') {
      for (; col < nextCol; col++) {
        this->addch(pixel, ' ');
        ++pixel.col;
      }
    } else {
      pixel = this->addnstr(pixel, &p[j], delta, propToFlag(pr[j]));
    }
    col = nextCol;
    j += delta;
  }

  if (this->somode) {
    this->somode = false;
    this->standend();
  }
  if (ulmode) {
    ulmode = false;
    this->underlineend();
  }
  if (bomode) {
    bomode = false;
    this->boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    this->boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    this->underlineend();
  }
  if (imag_mode) {
    imag_mode = false;
    this->standend();
  }
  if (form_mode) {
    form_mode = false;
    this->standend();
  }
  if (visited_mode) {
    visited_mode = false;
  }
  if (active_mode) {
    active_mode = false;
    this->boldend();
  }
  if (mark_mode) {
    mark_mode = false;
    this->standend();
  }
  if (graph_mode) {
    graph_mode = false;
    this->graphend();
  }
  return pixel;
}
