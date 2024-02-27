#include "line_layout.h"
#include "quote.h"
#include "etc.h"
#include "html/html_tag.h"
#include "linklist.h"
#include "app.h"
#include "url_quote.h"
#include "myctype.h"
#include "history.h"
#include "url.h"
#include "html/anchor.h"
#include "html/form.h"
#include "regex.h"
#include "alloc.h"

int nextpage_topline = false;

LineLayout::LineLayout() {
  this->_href = std::make_shared<AnchorList>();
  this->_name = std::make_shared<AnchorList>();
  this->_img = std::make_shared<AnchorList>();
  this->_formitem = std::make_shared<AnchorList>();
  this->_hmarklist = std::make_shared<HmarkerList>();
  this->_imarklist = std::make_shared<HmarkerList>();

  this->COLS = App::instance().COLS();
  this->LINES = App::instance().LASTLINE();
}

LineLayout::LineLayout(int width) : width(width) {
  this->COLS = App::instance().COLS();
  this->LINES = App::instance().LASTLINE();
}

void LineLayout::clearBuffer() {
  firstLine = topLine = currentLine = lastLine = nullptr;
  allLine = 0;

  this->_href->clear();
  this->_name->clear();
  this->_img->clear();
  this->_formitem->clear();
  this->formlist = nullptr;
  this->linklist = nullptr;
  this->maplist = nullptr;
  this->_hmarklist->clear();
  this->_imarklist->clear();
}

void LineLayout::addnewline(const char *line, Lineprop *prop, int byteLen,
                            int breakWidth, int realLinenum) {
  {
    auto l = new Line(++this->allLine, this->currentLine, line, prop, byteLen,
                      realLinenum);
    this->pushLine(l);
  }

  if (byteLen <= 0 || breakWidth <= 0) {
    return;
  }

  while (auto l = this->currentLine->breakLine(breakWidth)) {
    this->pushLine(l);
  }
}

Line *LineLayout::lineSkip(Line *line, int offset, int last) {
  auto l = line->currentLineSkip(offset, last);
  if (!nextpage_topline)
    for (int i = LINES - 1 - (lastLine->linenumber - l->linenumber);
         i > 0 && l->prev != NULL; i--, l = l->prev)
      ;
  return l;
}

void LineLayout::arrangeLine() {
  if (this->firstLine == NULL)
    return;
  this->cursorY = this->currentLine->linenumber - this->topLine->linenumber;
  auto i = this->currentLine->columnPos(this->currentColumn + this->visualpos -
                                        this->currentLine->bwidth);
  auto cpos = this->currentLine->bytePosToColumn(i) - this->currentColumn;
  if (cpos >= 0) {
    this->cursorX = cpos;
    this->pos = i;
  } else if (this->currentLine->len > i) {
    this->cursorX = 0;
    this->pos = i + 1;
  } else {
    this->cursorX = 0;
    this->pos = 0;
  }
#ifdef DISPLAY_DEBUG
  fprintf(stderr,
          "arrangeLine: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
          buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
          buf->currentLine->len);
#endif
}

/*
 * gotoLine: go to line number
 */
void LineLayout::gotoLine(int n) {
  char msg[36];
  Line *l = this->firstLine;

  if (l == nullptr)
    return;

  if (l->linenumber > n) {
    snprintf(msg, sizeof(msg), "First line is #%ld", l->linenumber);
    App::instance().set_delayed_message(msg);
    this->topLine = this->currentLine = l;
    return;
  }
  if (this->lastLine->linenumber < n) {
    l = this->lastLine;
    snprintf(msg, sizeof(msg), "Last line is #%ld", this->lastLine->linenumber);
    App::instance().set_delayed_message(msg);
    this->currentLine = l;
    this->topLine =
        this->lineSkip(this->currentLine, -(this->LINES - 1), false);
    return;
  }
  for (; l != nullptr; l = l->next) {
    if (l->linenumber >= n) {
      this->currentLine = l;
      if (n < this->topLine->linenumber ||
          this->topLine->linenumber + this->LINES <= n)
        this->topLine = this->lineSkip(l, -(this->LINES + 1) / 2, false);
      break;
    }
  }
}

void LineLayout::cursorUpDown(int n) {
  Line *cl = this->currentLine;

  if (this->firstLine == NULL)
    return;
  if ((this->currentLine = cl->currentLineSkip(n, false)) == cl)
    return;
  this->arrangeLine();
}

void LineLayout::cursorUp0(int n) {
  if (this->cursorY > 0)
    this->cursorUpDown(-1);
  else {
    this->topLine = this->lineSkip(this->topLine, -n, false);
    if (this->currentLine->prev != NULL)
      this->currentLine = this->currentLine->prev;
    this->arrangeLine();
  }
}

void LineLayout::cursorUp(int n) {
  Line *l = this->currentLine;
  if (this->firstLine == NULL)
    return;
  while (this->currentLine->prev && this->currentLine->bpos)
    cursorUp0(n);
  if (this->currentLine == this->firstLine) {
    this->gotoLine(l->linenumber);
    this->arrangeLine();
    return;
  }
  cursorUp0(n);
  while (this->currentLine->prev && this->currentLine->bpos &&
         this->currentLine->bwidth >= this->currentColumn + this->visualpos)
    cursorUp0(n);
}

void LineLayout::cursorDown0(int n) {
  if (this->cursorY < this->LINES - 1)
    this->cursorUpDown(1);
  else {
    this->topLine = this->lineSkip(this->topLine, n, false);
    if (this->currentLine->next != NULL)
      this->currentLine = this->currentLine->next;
    this->arrangeLine();
  }
}

void LineLayout::cursorDown(int n) {
  Line *l = this->currentLine;
  if (this->firstLine == NULL)
    return;
  while (this->currentLine->next && this->currentLine->next->bpos)
    cursorDown0(n);
  if (this->currentLine == this->lastLine) {
    this->gotoLine(l->linenumber);
    this->arrangeLine();
    return;
  }
  cursorDown0(n);
  while (this->currentLine->next && this->currentLine->next->bpos &&
         this->currentLine->bwidth + this->currentLine->width() <
             this->currentColumn + this->visualpos)
    cursorDown0(n);
}

int LineLayout::columnSkip(int offset) {
  int i, maxColumn;
  int column = this->currentColumn + offset;
  int nlines = this->LINES + 1;
  Line *l;

  maxColumn = 0;
  for (i = 0, l = this->topLine; i < nlines && l != NULL; i++, l = l->next) {
    if (l->width() - 1 > maxColumn) {
      maxColumn = l->width() - 1;
    }
  }
  maxColumn -= this->COLS - 1;
  if (column < maxColumn)
    maxColumn = column;
  if (maxColumn < 0)
    maxColumn = 0;

  if (this->currentColumn == maxColumn)
    return 0;
  this->currentColumn = maxColumn;
  return 1;
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void LineLayout::arrangeCursor() {
  int col, col2, pos;
  int delta = 1;
  if (this->currentLine == NULL) {
    return;
  }
  /* Arrange line */
  if (this->currentLine->linenumber - this->topLine->linenumber >=
          this->LINES ||
      this->currentLine->linenumber < this->topLine->linenumber) {
    /*
     * buf->topLine = buf->currentLine;
     */
    this->topLine = this->lineSkip(this->currentLine, 0, false);
  }
  /* Arrange column */
  while (this->pos < 0 && this->currentLine->prev && this->currentLine->bpos) {
    pos = this->pos + this->currentLine->prev->len;
    this->cursorUp0(1);
    this->pos = pos;
  }
  while (this->pos >= this->currentLine->len && this->currentLine->next &&
         this->currentLine->next->bpos) {
    pos = this->pos - this->currentLine->len;
    this->cursorDown0(1);
    this->pos = pos;
  }
  if (this->currentLine->len == 0 || this->pos < 0)
    this->pos = 0;
  else if (this->pos >= this->currentLine->len)
    this->pos = this->currentLine->len - 1;
  col = this->currentLine->bytePosToColumn(this->pos);
  col2 = this->currentLine->bytePosToColumn(this->pos + delta);
  if (col < this->currentColumn || col2 > this->COLS + this->currentColumn) {
    this->currentColumn = 0;
    if (col2 > this->COLS)
      columnSkip(col);
  }
  /* Arrange cursor */
  this->cursorY = this->currentLine->linenumber - this->topLine->linenumber;
  this->visualpos = this->currentLine->bwidth +
                    this->currentLine->bytePosToColumn(this->pos) -
                    this->currentColumn;
  this->cursorX = this->visualpos - this->currentLine->bwidth;
#ifdef DISPLAY_DEBUG
  fprintf(
      stderr,
      "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
      buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
      buf->currentLine->len);
#endif
}

void LineLayout::cursorRight(int n) {
  int i, delta = 1, cpos, vpos2;
  Line *l = this->currentLine;

  if (this->firstLine == NULL)
    return;
  if (this->pos == l->len && !(l->next && l->next->bpos))
    return;
  i = this->pos;
  if (i + delta < l->len) {
    this->pos = i + delta;
  } else if (l->len == 0) {
    this->pos = 0;
  } else if (l->next && l->next->bpos) {
    this->cursorDown0(1);
    this->pos = 0;
    this->arrangeCursor();
    return;
  } else {
    this->pos = l->len - 1;
  }
  cpos = l->bytePosToColumn(this->pos);
  this->visualpos = l->bwidth + cpos - this->currentColumn;
  delta = 1;
  vpos2 = l->bytePosToColumn(this->pos + delta) - this->currentColumn - 1;
  if (vpos2 >= this->COLS && n) {
    this->columnSkip(n + (vpos2 - this->COLS) - (vpos2 - this->COLS) % n);
    this->visualpos = l->bwidth + cpos - this->currentColumn;
  }
  this->cursorX = this->visualpos - l->bwidth;
}

void LineLayout::cursorLeft(int n) {
  int i, delta = 1, cpos;
  Line *l = this->currentLine;

  if (this->firstLine == NULL)
    return;
  i = this->pos;
  if (i >= delta)
    this->pos = i - delta;
  else if (l->prev && l->bpos) {
    this->cursorUp0(-1);
    this->pos = this->currentLine->len - 1;
    this->arrangeCursor();
    return;
  } else
    this->pos = 0;
  cpos = l->bytePosToColumn(this->pos);
  this->visualpos = l->bwidth + cpos - this->currentColumn;
  if (this->visualpos - l->bwidth < 0 && n) {
    this->columnSkip(-n + this->visualpos - l->bwidth -
                     (this->visualpos - l->bwidth) % n);
    this->visualpos = l->bwidth + cpos - this->currentColumn;
  }
  this->cursorX = this->visualpos - l->bwidth;
}

void LineLayout::cursorHome() {
  this->visualpos = 0;
  this->cursorX = this->cursorY = 0;
}

void LineLayout::cursorXY(int x, int y) {
  this->cursorUpDown(y - this->cursorY);

  if (this->cursorX > x) {
    while (this->cursorX > x) {
      this->cursorLeft(this->COLS / 2);
    }
  } else if (this->cursorX < x) {
    while (this->cursorX < x) {
      auto oldX = this->cursorX;

      this->cursorRight(this->COLS / 2);

      if (oldX == this->cursorX) {
        break;
      }
    }
    if (this->cursorX > x) {
      this->cursorLeft(this->COLS / 2);
    }
  }
}

void LineLayout::restorePosition(const LineLayout &orig) {
  this->topLine =
      this->lineSkip(this->firstLine, orig.TOP_LINENUMBER() - 1, false);
  this->gotoLine(orig.CUR_LINENUMBER());
  this->pos = orig.pos;
  if (this->currentLine && orig.currentLine)
    this->pos += orig.currentLine->bpos - this->currentLine->bpos;
  this->currentColumn = orig.currentColumn;
  this->arrangeCursor();
}

/*
 * gotoRealLine: go to real line number
 */
void LineLayout::gotoRealLine(int n) {
  Line *l = this->firstLine;
  if (l == nullptr) {
    return;
  }

  if (l->real_linenumber > n) {
    char msg[36];
    snprintf(msg, sizeof(msg), "First line is #%ld", l->real_linenumber);
    App::instance().set_delayed_message(msg);
    this->topLine = this->currentLine = l;
    return;
  }

  if (this->lastLine->real_linenumber < n) {
    l = this->lastLine;
    char msg[36];
    snprintf(msg, sizeof(msg), "Last line is #%ld",
             this->lastLine->real_linenumber);
    App::instance().set_delayed_message(msg);
    this->currentLine = l;
    this->topLine =
        this->lineSkip(this->currentLine, -(this->LINES - 1), false);
    return;
  }

  for (; l != nullptr; l = l->next) {
    if (l->real_linenumber >= n) {
      this->currentLine = l;
      if (n < this->topLine->real_linenumber ||
          this->topLine->real_linenumber + this->LINES <= n)
        this->topLine = this->lineSkip(l, -(this->LINES + 1) / 2, false);
      break;
    }
  }
}

Anchor *LineLayout::registerForm(HtmlParser *parser, FormList *flist,
                                 HtmlTag *tag, int line, int pos) {
  auto fi = flist->formList_addInput(parser, tag);
  if (!fi) {
    return NULL;
  }
  return this->formitem()->putAnchor((const char *)fi, flist->target, {}, NULL,
                                     '\0', line, pos);
}

void LineLayout::addMultirowsForm(AnchorList *al) {
  int j, k, col, ecol, pos;
  Anchor a_form, *a;
  Line *l, *ls;

  if (al == NULL || al->size() == 0)
    return;
  for (size_t i = 0; i < al->size(); i++) {
    a_form = al->anchors[i];
    al->anchors[i].rows = 1;
    if (a_form.hseq < 0 || a_form.rows <= 1)
      continue;
    for (l = this->firstLine; l != NULL; l = l->next) {
      if (l->linenumber == a_form.y)
        break;
    }
    if (!l)
      continue;
    if (a_form.y == a_form.start.line)
      ls = l;
    else {
      for (ls = l; ls != NULL;
           ls = (a_form.y < a_form.start.line) ? ls->next : ls->prev) {
        if (ls->linenumber == a_form.start.line)
          break;
      }
      if (!ls)
        continue;
    }
    col = ls->bytePosToColumn(a_form.start.pos);
    ecol = ls->bytePosToColumn(a_form.end.pos);
    for (j = 0; l && j < a_form.rows; l = l->next, j++) {
      pos = l->columnPos(col);
      if (j == 0) {
        this->hmarklist()->marks[a_form.hseq].line = l->linenumber;
        this->hmarklist()->marks[a_form.hseq].pos = pos;
      }
      if (a_form.start.line == l->linenumber)
        continue;
      a = this->formitem()->putAnchor(a_form.url, a_form.target.c_str(), {},
                                      NULL, '\0', l->linenumber, pos);
      a->hseq = a_form.hseq;
      a->y = a_form.y;
      a->end.pos = pos + ecol - col;
      if (pos < 1 || a->end.pos >= l->size())
        continue;
      l->lineBuf[pos - 1] = '[';
      l->lineBuf[a->end.pos] = ']';
      for (k = pos; k < a->end.pos; k++)
        l->propBuf[k] |= PE_FORM;
    }
  }
}

/* go to the next downward/upward anchor */
void LineLayout::nextY(int d, int n) {
  if (!this->firstLine) {
    return;
  }

  auto hl = this->hmarklist();
  if (hl->size() == 0) {
    return;
  }

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = this->retrieveCurrentForm();
  }

  int x = this->pos;
  int y = this->currentLine->linenumber + d;
  Anchor *pan = nullptr;
  int hseq = -1;
  for (int i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = nullptr;
    for (; y >= 0 && y <= this->lastLine->linenumber; y += d) {
      if (this->href()) {
        an = this->href()->retrieveAnchor(y, x);
      }
      if (!an && this->formitem()) {
        an = this->formitem()->retrieveAnchor(y, x);
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
  this->gotoLine(pan->start.line);
  this->arrangeLine();
  App::instance().invalidate();
}

/* go to the next left/right anchor */
void LineLayout::nextX(int d, int dy, int n) {
  if (this->firstLine == nullptr)
    return;

  auto hl = this->hmarklist();
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = retrieveCurrentForm();
  }

  auto l = this->currentLine;
  auto x = this->pos;
  auto y = l->linenumber;
  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = nullptr;
    while (1) {
      for (; x >= 0 && x < l->len; x += d) {
        if (this->href()) {
          an = this->href()->retrieveAnchor(y, x);
        }
        if (!an && this->formitem()) {
          an = this->formitem()->retrieveAnchor(y, x);
        }
        if (an) {
          pan = an;
          break;
        }
      }
      if (!dy || an)
        break;
      l = (dy > 0) ? l->next : l->prev;
      if (!l)
        break;
      x = (d > 0) ? 0 : l->len - 1;
      y = l->linenumber;
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  this->gotoLine(y);
  this->pos = pan->start.pos;
  this->arrangeCursor();
  App::instance().invalidate();
}

/* go to the previous anchor */
void LineLayout::_prevA(bool visited, std::optional<Url> baseUrl, int n) {
  auto hl = this->hmarklist();
  BufferPoint *po;
  Anchor *pan;
  int i, x, y;
  Url url;

  if (this->firstLine == nullptr)
    return;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (visited != true && an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  y = this->currentLine->linenumber;
  x = this->pos;

  if (visited == true) {
    n = hl->size();
  }

  for (i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq - 1;
      do {
        if (hseq < 0) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        po = &hl->marks[hseq];
        if (this->href()) {
          an = this->href()->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && this->formitem()) {
          an = this->formitem()->retrieveAnchor(po->line, po->pos);
        }
        hseq--;
        if (visited == true && an) {
          url = urlParse(an->url, baseUrl);
          if (URLHist->getHashHist(url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = this->href()->closest_prev_anchor(nullptr, x, y);
      if (visited != true)
        an = this->formitem()->closest_prev_anchor(an, x, y);
      if (an == nullptr) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true && an) {
        url = urlParse(an->url, baseUrl);
        if (URLHist->getHashHist(url.to_Str().c_str())) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == nullptr || an->hseq < 0)
    return;
  po = &hl->marks[an->hseq];
  this->gotoLine(po->line);
  this->pos = po->pos;
  this->arrangeCursor();
  App::instance().invalidate();
}

/* go to the next [visited] anchor */
void LineLayout::_nextA(bool visited, std::optional<Url> baseUrl, int n) {
  if (this->firstLine == nullptr)
    return;

  auto hl = this->hmarklist();
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (visited != true && an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  auto y = this->currentLine->linenumber;
  auto x = this->pos;

  if (visited == true) {
    n = hl->size();
  }

  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq + 1;
      do {
        if (hseq >= (int)hl->size()) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        auto po = &hl->marks[hseq];
        if (this->href()) {
          an = this->href()->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && this->formitem()) {
          an = this->formitem()->retrieveAnchor(po->line, po->pos);
        }
        hseq++;
        if (visited == true && an) {
          auto url = urlParse(an->url, baseUrl);
          if (URLHist->getHashHist(url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = this->href()->closest_next_anchor(nullptr, x, y);
      if (visited != true)
        an = this->formitem()->closest_next_anchor(an, x, y);
      if (an == nullptr) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true) {
        auto url = urlParse(an->url, baseUrl);
        if (URLHist->getHashHist(url.to_Str().c_str())) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == nullptr || an->hseq < 0)
    return;
  auto po = &hl->marks[an->hseq];
  this->gotoLine(po->line);
  this->pos = po->pos;
  this->arrangeCursor();
  App::instance().invalidate();
}

/* Move cursor left */
void LineLayout::_movL(int n, int m) {
  if (this->firstLine == nullptr)
    return;
  for (int i = 0; i < m; i++) {
    this->cursorLeft(n);
  }
  App::instance().invalidate();
}

/* Move cursor downward */
void LineLayout::_movD(int n, int m) {
  if (this->firstLine == nullptr) {
    return;
  }
  for (int i = 0; i < m; i++) {
    this->cursorDown(n);
  }
  App::instance().invalidate();
}

/* move cursor upward */
void LineLayout::_movU(int n, int m) {
  if (this->firstLine == nullptr) {
    return;
  }
  for (int i = 0; i < m; i++) {
    this->cursorUp(n);
  }
  App::instance().invalidate();
}

/* Move cursor right */
void LineLayout::_movR(int n, int m) {
  if (this->firstLine == nullptr)
    return;
  for (int i = 0; i < m; i++) {
    this->cursorRight(n);
  }
  App::instance().invalidate();
}

int LineLayout::prev_nonnull_line(Line *line) {
  Line *l;
  for (l = line; l != nullptr && l->len == 0; l = l->prev)
    ;
  if (l == nullptr || l->len == 0)
    return -1;

  this->currentLine = l;
  if (l != line)
    this->pos = this->currentLine->len;
  return 0;
}

int LineLayout::next_nonnull_line(Line *line) {
  Line *l;
  for (l = line; l != nullptr && l->len == 0; l = l->next)
    ;
  if (l == nullptr || l->len == 0)
    return -1;

  this->currentLine = l;
  if (l != line)
    this->pos = 0;
  return 0;
}

/* Go to specified line */
void LineLayout::_goLine(const char *l, int prec_num) {
  if (l == nullptr || *l == '\0' || this->currentLine == nullptr) {
    App::instance().invalidate();
    return;
  }

  this->pos = 0;
  if (((*l == '^') || (*l == '$')) && prec_num) {
    this->gotoRealLine(prec_num);
  } else if (*l == '^') {
    this->topLine = this->currentLine = this->firstLine;
  } else if (*l == '$') {
    this->topLine =
        this->lineSkip(this->lastLine, -(this->LINES + 1) / 2, true);
    this->currentLine = this->lastLine;
  } else
    this->gotoRealLine(atoi(l));
  this->arrangeCursor();
  App::instance().invalidate();
}

void LineLayout::shiftvisualpos(int shift) {
  Line *l = this->currentLine;
  this->visualpos -= shift;
  if (this->visualpos - l->bwidth >= this->COLS)
    this->visualpos = l->bwidth + this->COLS - 1;
  else if (this->visualpos - l->bwidth < 0)
    this->visualpos = l->bwidth;
  this->arrangeLine();
  if (this->visualpos - l->bwidth == -shift && this->cursorX == 0)
    this->visualpos = l->bwidth;
}

std::string LineLayout::getCurWord(int *spos, int *epos) const {
  Line *l = this->currentLine;
  if (l == nullptr)
    return {};

  *spos = 0;
  *epos = 0;
  auto &p = l->lineBuf;
  auto e = this->pos;
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
  while (e < l->len && is_wordchar(p[e])) {
    e++;
  }
  *spos = b;
  *epos = e;
  return {p.data() + b, p.data() + e};
}

/*
 * Command functions: These functions are called with a keystroke.
 */
void LineLayout::nscroll(int n) {
  if (this->firstLine == nullptr) {
    return;
  }

  auto top = this->topLine;
  auto cur = this->currentLine;

  auto lnum = cur->linenumber;
  this->topLine = this->lineSkip(top, n, false);
  if (this->topLine == top) {
    lnum += n;
    if (lnum < this->topLine->linenumber)
      lnum = this->topLine->linenumber;
    else if (lnum > this->lastLine->linenumber)
      lnum = this->lastLine->linenumber;
  } else {
    auto tlnum = this->topLine->linenumber;
    auto llnum = this->topLine->linenumber + this->LINES - 1;
    int diff_n;
    if (nextpage_topline)
      diff_n = 0;
    else
      diff_n = n - (tlnum - top->linenumber);
    if (lnum < tlnum)
      lnum = tlnum + diff_n;
    if (lnum > llnum)
      lnum = llnum + diff_n;
  }
  this->gotoLine(lnum);
  this->arrangeLine();
  if (n > 0) {
    if (this->currentLine->bpos &&
        this->currentLine->bwidth >= this->currentColumn + this->visualpos)
      this->cursorDown(1);
    else {
      while (this->currentLine->next && this->currentLine->next->bpos &&
             this->currentLine->bwidth + this->currentLine->width() <
                 this->currentColumn + this->visualpos)
        this->cursorDown0(1);
    }
  } else {
    if (this->currentLine->bwidth + this->currentLine->width() <
        this->currentColumn + this->visualpos)
      this->cursorUp(1);
    else {
      while (this->currentLine->prev && this->currentLine->bpos &&
             this->currentLine->bwidth >= this->currentColumn + this->visualpos)
        this->cursorUp0(1);
    }
  }
  // App::instance().invalidate(mode);
}

/* renumber anchor */
void LineLayout::reseq_anchor() {
  int nmark = this->hmarklist()->size();
  int n = nmark;
  for (size_t i = 0; i < this->href()->size(); i++) {
    auto a = &this->href()->anchors[i];
    if (a->hseq == -2) {
      n++;
    }
  }
  if (n == nmark) {
    return;
  }

  auto seqmap = std::vector<short>(n);
  for (int i = 0; i < n; i++) {
    seqmap[i] = i;
  }

  n = nmark;
  for (size_t i = 0; i < this->href()->size(); i++) {
    auto a = &this->href()->anchors[i];
    if (a->hseq == -2) {
      a->hseq = n;
      auto a1 =
          this->href()->closest_next_anchor(NULL, a->start.pos, a->start.line);
      a1 = this->formitem()->closest_next_anchor(a1, a->start.pos,
                                                 a->start.line);
      if (a1 && a1->hseq >= 0) {
        seqmap[n] = seqmap[a1->hseq];
        for (int j = a1->hseq; j < nmark; j++) {
          seqmap[j]++;
        }
      }
      this->hmarklist()->putHmarker(a->start.line, a->start.pos, seqmap[n]);
      n++;
    }
  }

  for (int i = 0; i < nmark; i++) {
    this->hmarklist()->putHmarker(this->hmarklist()->marks[i].line,
                                  this->hmarklist()->marks[i].pos, seqmap[i]);
  }

  this->href()->reseq_anchor0(seqmap.data());
  this->formitem()->reseq_anchor0(seqmap.data());
}

const char *LineLayout ::reAnchorPos(
    Line *l, const char *p1, const char *p2,
    Anchor *(*anchorproc)(LineLayout *, const char *, const char *, int, int)) {
  Anchor *a;
  int spos, epos;
  int i;
  int hseq = -2;

  spos = p1 - l->lineBuf.data();
  epos = p2 - l->lineBuf.data();
  for (i = spos; i < epos; i++) {
    if (l->propBuf[i] & (PE_ANCHOR | PE_FORM))
      return p2;
  }
  for (i = spos; i < epos; i++)
    l->propBuf[i] |= PE_ANCHOR;
  while (spos > l->len && l->next && l->next->bpos) {
    spos -= l->len;
    epos -= l->len;
    l = l->next;
  }
  while (1) {
    a = anchorproc(this, p1, p2, l->linenumber, spos);
    a->hseq = hseq;
    if (hseq == -2) {
      this->reseq_anchor();
      hseq = a->hseq;
    }
    a->end.line = l->linenumber;
    if (epos > l->len && l->next && l->next->bpos) {
      a->end.pos = l->len;
      spos = 0;
      epos -= l->len;
      l = l->next;
    } else {
      a->end.pos = epos;
      break;
    }
  }
  return p2;
}

/* search regexp and register them as anchors */
/* returns error message if any               */
const char *LineLayout::reAnchorAny(
    const char *re,
    Anchor *(*anchorproc)(LineLayout *, const char *, const char *, int, int)) {
  Line *l;
  const char *p = NULL, *p1, *p2;

  if (re == NULL || *re == '\0') {
    return NULL;
  }
  if ((re = regexCompile(re, 1)) != NULL) {
    return re;
  }
  for (l = MarkAllPages ? this->firstLine : this->topLine;
       l != NULL &&
       (MarkAllPages ||
        l->linenumber < this->topLine->linenumber + App::instance().LASTLINE());
       l = l->next) {
    if (p && l->bpos) {
      continue;
    }
    p = l->lineBuf.data();
    for (;;) {
      if (regexMatch(p, &l->lineBuf[l->size()] - p, p == l->lineBuf.data()) ==
          1) {
        matchedPosition(&p1, &p2);
        p = this->reAnchorPos(l, p1, p2, anchorproc);
      } else
        break;
    }
  }
  return NULL;
}

static Anchor *_put_anchor_all(LineLayout *layout, const char *p1,
                               const char *p2, int line, int pos) {
  auto tmp = Strnew_charp_n(p1, p2 - p1);
  return layout->href()->putAnchor(url_quote(tmp->ptr).c_str(), NULL,
                                   {.no_referer = true}, NULL, '\0', line, pos);
}

const char *LineLayout::reAnchor(const char *re) {
  return this->reAnchorAny(re, _put_anchor_all);
}

const char *LineLayout::reAnchorWord(Line *l, int spos, int epos) {
  return this->reAnchorPos(l, &l->lineBuf[spos], &l->lineBuf[epos],
                           _put_anchor_all);
}

Anchor *LineLayout::retrieveCurrentAnchor() {
  if (!this->currentLine || !this->href())
    return NULL;
  return this->href()->retrieveAnchor(this->currentLine->linenumber, this->pos);
}

Anchor *LineLayout::retrieveCurrentImg() {
  if (!this->currentLine || !this->img())
    return NULL;
  return this->img()->retrieveAnchor(this->currentLine->linenumber, this->pos);
}

Anchor *LineLayout::retrieveCurrentForm() {
  if (!this->currentLine || !this->formitem())
    return NULL;
  return this->formitem()->retrieveAnchor(this->currentLine->linenumber,
                                          this->pos);
}

const char *LineLayout::getAnchorText(AnchorList *al, Anchor *a) {
  if (!a || a->hseq < 0)
    return NULL;

  Str *tmp = NULL;
  auto hseq = a->hseq;
  auto l = this->firstLine;
  for (size_t i = 0; i < al->size(); i++) {
    a = &al->anchors[i];
    if (a->hseq != hseq)
      continue;
    for (; l; l = l->next) {
      if (l->linenumber == a->start.line)
        break;
    }
    if (!l)
      break;
    auto p = l->lineBuf.data() + a->start.pos;
    auto ep = l->lineBuf.data() + a->end.pos;
    for (; p < ep && IS_SPACE(*p); p++)
      ;
    if (p == ep)
      continue;
    if (!tmp)
      tmp = Strnew_size(ep - p);
    else
      Strcat_char(tmp, ' ');
    Strcat_charp_n(tmp, p, ep - p);
  }
  return tmp ? tmp->ptr : NULL;
}

void LineLayout::save_buffer_position() {
  BufferPos *b = this->undo;

  if (!this->firstLine)
    return;
  if (b && b->top_linenumber == this->TOP_LINENUMBER() &&
      b->cur_linenumber == this->CUR_LINENUMBER() &&
      b->currentColumn == this->currentColumn && b->pos == this->pos)
    return;
  b = (BufferPos *)New(BufferPos);
  b->top_linenumber = this->TOP_LINENUMBER();
  b->cur_linenumber = this->CUR_LINENUMBER();
  b->currentColumn = this->currentColumn;
  b->pos = this->pos;
  b->bpos = this->currentLine ? this->currentLine->bpos : 0;
  b->next = NULL;
  b->prev = this->undo;
  if (this->undo)
    this->undo->next = b;
  this->undo = b;
}

void LineLayout::resetPos(BufferPos *b) {
  LineLayout buf;
  auto top = new Line(b->top_linenumber);
  buf.topLine = top;

  auto cur = new Line(b->cur_linenumber);
  cur->bpos = b->bpos;

  buf.currentLine = cur;
  buf.pos = b->pos;
  buf.currentColumn = b->currentColumn;
  this->restorePosition(buf);
  this->undo = b;
  App::instance().invalidate();
}

void LineLayout::undoPos(int n) {
  BufferPos *b = this->undo;
  if (!b || !b->prev)
    return;
  for (int i = 0; i < n && b->prev; i++, b = b->prev)
    ;
  this->resetPos(b);
}

void LineLayout::redoPos(int n) {
  BufferPos *b = this->undo;
  if (!b || !b->next)
    return;
  for (int i = 0; i < n && b->next; i++, b = b->next)
    ;
  this->resetPos(b);
}

void LineLayout::addLink(struct HtmlTag *tag) {
  const char *href = NULL, *title = NULL, *ctype = NULL, *rel = NULL,
             *rev = NULL;
  char type = LINK_TYPE_NONE;
  LinkList *l;

  tag->parsedtag_get_value(ATTR_HREF, &href);
  if (href)
    href = Strnew(url_quote(remove_space(href)))->ptr;
  tag->parsedtag_get_value(ATTR_TITLE, &title);
  tag->parsedtag_get_value(ATTR_TYPE, &ctype);
  tag->parsedtag_get_value(ATTR_REL, &rel);
  if (rel != NULL) {
    /* forward link type */
    type = LINK_TYPE_REL;
    if (title == NULL)
      title = rel;
  }
  tag->parsedtag_get_value(ATTR_REV, &rev);
  if (rev != NULL) {
    /* reverse link type */
    type = LINK_TYPE_REV;
    if (title == NULL)
      title = rev;
  }

  l = (LinkList *)New(LinkList);
  l->url = href;
  l->title = title;
  l->ctype = ctype;
  l->type = type;
  l->next = NULL;
  if (this->linklist) {
    LinkList *i;
    for (i = this->linklist; i->next; i = i->next)
      ;
    i->next = l;
  } else
    this->linklist = l;
}

/* mark URL-like patterns as anchors */
void LineLayout::chkURLBuffer() {
  static const char *url_like_pat[] = {
      "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/"
      "=\\-]",
      "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
      "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
      "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
#ifdef INET6
      "https?://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./"
      "?=~_\\&+@#,\\$;]*",
      "ftp://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif /* INET6 */
      nullptr};
  for (int i = 0; url_like_pat[i]; i++) {
    this->reAnchor(url_like_pat[i]);
  }
  this->check_url = true;
}

void LineLayout::reshape(int width, const LineLayout &sbuf) {
  if (!this->need_reshape) {
    return;
  }
  this->need_reshape = false;
  this->width = width;

  this->height = App::instance().LASTLINE() + 1;
  if (this->firstLine && sbuf.firstLine) {
    Line *cur = sbuf.currentLine;
    int n;

    this->pos = sbuf.pos + cur->bpos;
    while (cur->bpos && cur->prev)
      cur = cur->prev;
    if (cur->real_linenumber > 0) {
      this->gotoRealLine(cur->real_linenumber);
    } else {
      this->gotoLine(cur->linenumber);
    }
    n = (this->currentLine->linenumber - this->topLine->linenumber) -
        (cur->linenumber - sbuf.topLine->linenumber);
    if (n) {
      this->topLine = this->lineSkip(this->topLine, n, false);
      if (cur->real_linenumber > 0) {
        this->gotoRealLine(cur->real_linenumber);
      } else {
        this->gotoLine(cur->linenumber);
      }
    }
    this->pos -= this->currentLine->bpos;
    this->currentColumn = sbuf.currentColumn;
    this->arrangeCursor();
  }
  if (this->check_url) {
    this->chkURLBuffer();
  }
  formResetBuffer(this, sbuf.formitem().get());
}

int LineLayout::cur_real_linenumber() const {

  auto cur = this->currentLine;
  if (!cur) {
    return 1;
  }

  int n = cur->real_linenumber ? cur->real_linenumber : 1;
  for (auto l = this->firstLine; l && l != cur && l->real_linenumber == 0;
       l = l->next) { /* header */
    if (l->bpos == 0)
      n++;
  }
  return n;
}
