#include "screen.h"
#include "line_layout.h"
#include "html/anchorlist.h"
#include <ftxui/screen/screen.hpp>

void Screen::setupscreen(const RowCol &size) {
  _screen = std::make_shared<ftxui::Screen>(size.col, size.row);
}

void Screen::clear(void) {
  _screen->Clear();
  CurrentMode = ScreenFlags::ASCII;
}

void Screen::clrtoeol(const RowCol &pos) { /* Clear to the end of line */
  if (pos.row >= 0 && pos.row < LINES()) {
    int y = pos.row + 1;
    for (int x = pos.col + 1; x <= COLS(); ++x) {
      _screen->PixelAt(x, y) = {};
    }
  }
}

void Screen::clrtobot_eol(
    const RowCol &pos, const std::function<void(const RowCol &pos)> &callback) {
  clrtoeol(pos);
  int CurLine = pos.row + 1;
  for (; CurLine < LINES(); CurLine++) {
    callback({.row = CurLine, .col = 0});
  }
}

RowCol Screen::addstr(RowCol pos, const char *s) {
  while (*s) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
  }
  return pos;
}

RowCol Screen::addnstr(RowCol pos, const char *s, int n) {
  for (int i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
    i += utf8.view().size();
  }
  return pos;
}

RowCol Screen::addnstr_sup(RowCol pos, const char *s, int n) {
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

void Screen::toggle_stand(const RowCol &pos) {
  auto &pixel = this->pixel(pos);
  pixel.inverted = !pixel.inverted;
}

void Screen::addmch(const RowCol &pos, const Utf8 &utf8) {
  auto &pixel = this->pixel(pos);
  pixel.character = utf8.view();
}

Line *Screen::redrawLine(LineLayout *buf, Line *l, int i) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->currentColumn;
  char *p;

  if (l == NULL) {
    return NULL;
  }
  RowCol pixel{.row = i, .col = buf->rootX};
  if (l->len() == 0 || l->width() - 1 < column) {
    this->clrtoeolx(pixel);
    return l;
  }
  /* need_clrtoeol(); */
  pos = l->columnPos(column);
  p = &(l->lineBuf[pos]);
  // pr = &(l->propBuf[pos]);
  rcol = l->bytePosToColumn(pos);

  for (j = 0; rcol - column < buf->COLS && pos + j < l->len(); j += delta) {
    delta = get_mclen(&p[j]);
    ncol = l->bytePosToColumn(pos + j + delta);
    if (ncol - column > buf->COLS)
      break;
    if (rcol < column) {
      for (rcol = column; rcol < ncol; rcol++) {
        this->addch(pixel, ' ');
        ++pixel.col;
      }
      continue;
    }
    if (p[j] == '\t') {
      for (; rcol < ncol; rcol++) {
        this->addch(pixel, ' ');
        ++pixel.col;
      }
    } else {
      pixel = this->addnstr(pixel, &p[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
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
  if (rcol - column < buf->COLS)
    this->clrtoeolx(pixel);
  return l;
}

void Screen::redrawNLine(LineLayout *layout, int n) {

  Line *l;
  int i;
  for (i = 0, l = layout->topLine; i < layout->LINES; i++, ++l) {
    if (i >= layout->LINES - n || i < -n)
      l = this->redrawLine(layout, l, i + layout->rootY);
    if (l == NULL)
      break;
  }
  if (n > 0) {
    this->clrtobotx({.row = i + layout->rootY, .col = 0});
  }
}

int Screen::redrawLineRegion(LineLayout *layout, Line *l, int i, int bpos,
                             int epos) {
  int j, pos, rcol, ncol, delta = 1;
  int column = layout->currentColumn;
  char *p;
  // Lineprop *pr;
  int bcol, ecol;

  if (l == NULL)
    return 0;
  pos = l->columnPos(column);
  p = &(l->lineBuf[pos]);
  // pr = &(l->propBuf[pos]);
  rcol = l->bytePosToColumn(pos);
  bcol = bpos - pos;
  ecol = epos - pos;

  for (j = 0; rcol - column < layout->COLS && pos + j < l->len(); j += delta) {
    delta = get_mclen(&p[j]);
    ncol = l->bytePosToColumn(pos + j + delta);
    if (ncol - column > layout->COLS)
      break;
    if (j >= bcol && j < ecol) {
      if (rcol < column) {
        RowCol pixel{.row = i, .col = layout->rootX};
        for (rcol = column; rcol < ncol; rcol++) {
          this->addch(pixel, ' ');
          ++pixel.col;
        }
        continue;
      }
      RowCol pixel{.row = i, .col = rcol - column + layout->rootX};
      if (p[j] == '\t') {
        for (; rcol < ncol; rcol++) {
          this->addch(pixel, ' ');
          ++pixel.col;
        }
      } else
        pixel = this->addnstr(pixel, &p[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
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
  return rcol - column;
}

void Screen::drawAnchorCursor0(LineLayout *layout, AnchorList<Anchor> *al,
                               int hseq, int prevhseq, int tline, int eline,
                               int active) {
  auto l = layout->topLine;
  for (size_t j = 0; j < al->size(); j++) {
    auto an = &al->anchors[j];
    if (an->start.line < tline)
      continue;
    if (an->start.line >= eline)
      return;
    for (;; ++l) {
      if (layout->isNull(l))
        return;
      if (layout->linenumber(l) == an->start.line)
        break;
    }
    if (hseq >= 0 && an->hseq == hseq) {
      int start_pos = an->start.pos;
      int end_pos = an->end.pos;
      for (int i = an->start.pos; i < an->end.pos; i++) {
        if (l->propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM)) {
          if (active)
            l->propBuf[i] |= PE_ACTIVE;
          else
            l->propBuf[i] &= ~PE_ACTIVE;
        }
      }
      if (active && start_pos < end_pos)
        this->redrawLineRegion(layout, l,
                               layout->linenumber(l) - tline + layout->rootY,
                               start_pos, end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
      if (active)
        this->redrawLineRegion(layout, l,
                               layout->linenumber(l) - tline + layout->rootY,
                               an->start.pos, an->end.pos);
    }
  }
}

void Screen ::drawAnchorCursor(LineLayout *layout) {
  int hseq, prevhseq;
  int tline, eline;

  if (layout->empty() || !layout->data._hmarklist)
    return;
  if (!layout->data._href && !layout->data._formitem)
    return;

  auto an = layout->retrieveCurrentAnchor();
  if (an) {
    hseq = an->hseq;
  } else {
    hseq = -1;
  }
  tline = layout->linenumber(layout->topLine);
  eline = tline + layout->LINES;
  prevhseq = layout->data._hmarklist->prevhseq;

  if (layout->data._href) {
    this->drawAnchorCursor0(layout, layout->data._href.get(), hseq, prevhseq,
                            tline, eline, 1);
    this->drawAnchorCursor0(layout, layout->data._href.get(), hseq, -1, tline,
                            eline, 0);
  }
  if (layout->data._formitem) {
    this->drawAnchorCursor0(layout, layout->data._formitem.get(), hseq,
                            prevhseq, tline, eline, 1);
    this->drawAnchorCursor0(layout, layout->data._formitem.get(), hseq, -1,
                            tline, eline, 0);
  }
  layout->data._hmarklist->prevhseq = hseq;
}

std::string Screen::str(LineLayout *layout) {
  if (cline != layout->topLine || ccolumn != layout->currentColumn) {
    this->redrawNLine(layout, this->LASTLINE());
    cline = layout->topLine;
    ccolumn = layout->currentColumn;
  }
  assert(layout->topLine);

  this->drawAnchorCursor(layout);

  return this->_screen->ToString();
}
