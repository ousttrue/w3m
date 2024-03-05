#include "line_layout.h"
#include "app.h"
#include "myctype.h"
#include "history.h"
#include "url.h"
#include "html/anchor.h"
#include "html/form.h"
#include "regex.h"
#include "alloc.h"

int nextpage_topline = false;

LineLayout::LineLayout() {
  this->COLS = App::instance().COLS();
  this->LINES = App::instance().LASTLINE();
}

LineLayout::LineLayout(int width) : width(width) {
  this->COLS = App::instance().COLS();
  this->LINES = App::instance().LASTLINE();
}

void LineLayout::clearBuffer() {
  data.clear();
  topLine = currentLine = nullptr;
}

Line *LineLayout::lineSkip(Line *line, int offset) {
  auto l = currentLineSkip(line, offset);
  if (!nextpage_topline)
    for (int i = LINES - 1 - (linenumber(lastLine()) - linenumber(l));
         i > 0 && hasPrev(l); i--, --l)
      ;
  return l;
}

void LineLayout::arrangeLine() {
  if (this->empty())
    return;
  this->cursorY = linenumber(currentLine) - linenumber(topLine);
  auto i = this->currentLine->columnPos(this->currentColumn + this->visualpos -
                                        this->currentLine->width());
  auto cpos = this->currentLine->bytePosToColumn(i) - this->currentColumn;
  if (cpos >= 0) {
    this->cursorX = cpos;
    this->pos = i;
  } else if (this->currentLine->len() > i) {
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
          buf->currentLine->len());
#endif
}

/*
 * gotoLine: go to line number
 */
void LineLayout::gotoLine(int n) {
  if (empty())
    return;

  char msg[36];
  Line *l = this->firstLine();
  if (linenumber(l) > n) {
    snprintf(msg, sizeof(msg), "First line is #%d", linenumber(l));
    App::instance().set_delayed_message(msg);
    this->topLine = this->currentLine = l;
    return;
  }
  if (linenumber(lastLine()) < n) {
    l = this->lastLine();
    snprintf(msg, sizeof(msg), "Last line is #%d", linenumber(lastLine()));
    App::instance().set_delayed_message(msg);
    this->currentLine = l;
    this->topLine = this->lineSkip(this->currentLine, -(this->LINES - 1));
    return;
  }
  for (; l != nullptr; ++l) {
    if (linenumber(l) >= n) {
      this->currentLine = l;
      if (n < linenumber(topLine) || linenumber(topLine) + this->LINES <= n)
        this->topLine = this->lineSkip(l, -(this->LINES + 1) / 2);
      break;
    }
  }
}

void LineLayout::cursorUpDown(int n) {
  if (this->empty())
    return;

  Line *cl = this->currentLine;
  if ((this->currentLine = currentLineSkip(cl, n)) == cl)
    return;
  this->arrangeLine();
}

void LineLayout::cursorUp0(int n) {
  if (this->cursorY > 0)
    this->cursorUpDown(-1);
  else {
    this->topLine = this->lineSkip(this->topLine, -n);
    if (hasPrev(this->currentLine))
      --this->currentLine;
    this->arrangeLine();
  }
}

void LineLayout::cursorUp(int n) {
  if (this->empty())
    return;

  Line *l = this->currentLine;
  if (this->currentLine == this->firstLine()) {
    this->gotoLine(linenumber(l));
    this->arrangeLine();
    return;
  }
  cursorUp0(n);
}

void LineLayout::cursorDown0(int n) {
  if (this->cursorY < this->LINES - 1)
    this->cursorUpDown(1);
  else {
    this->topLine = this->lineSkip(this->topLine, n);
    if (hasNext(this->currentLine))
      ++this->currentLine;
    this->arrangeLine();
  }
}

void LineLayout::cursorDown(int n) {
  if (empty())
    return;

  Line *l = this->currentLine;
  if (this->currentLine == this->lastLine()) {
    this->gotoLine(linenumber(l));
    this->arrangeLine();
    return;
  }
  cursorDown0(n);
}

int LineLayout::columnSkip(int offset) {
  int i, maxColumn;
  int column = this->currentColumn + offset;
  int nlines = this->LINES + 1;
  Line *l;

  maxColumn = 0;
  for (i = 0, l = this->topLine; i < nlines && l != NULL; i++, ++l) {
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
  int col, col2;
  int delta = 1;
  if (this->currentLine == NULL) {
    return;
  }
  /* Arrange line */
  if (linenumber(currentLine) - linenumber(topLine) >= this->LINES ||
      linenumber(currentLine) < linenumber(topLine)) {
    /*
     * buf->topLine = buf->currentLine;
     */
    this->topLine = this->lineSkip(this->currentLine, 0);
  }
  /* Arrange column */
  if (this->currentLine->len() == 0 || this->pos < 0)
    this->pos = 0;
  else if (this->pos >= this->currentLine->len())
    this->pos = this->currentLine->len() - 1;
  col = this->currentLine->bytePosToColumn(this->pos);
  col2 = this->currentLine->bytePosToColumn(this->pos + delta);
  if (col < this->currentColumn || col2 > this->COLS + this->currentColumn) {
    this->currentColumn = 0;
    if (col2 > this->COLS)
      columnSkip(col);
  }
  /* Arrange cursor */
  this->cursorY = linenumber(currentLine) - linenumber(topLine);
  this->visualpos = this->currentLine->width() +
                    this->currentLine->bytePosToColumn(this->pos) -
                    this->currentColumn;
  this->cursorX = this->visualpos - this->currentLine->width();
#ifdef DISPLAY_DEBUG
  fprintf(
      stderr,
      "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
      buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
      buf->currentLine->len());
#endif
}

void LineLayout::cursorRight(int n) {
  if (empty())
    return;

  int i, delta = 1, cpos, vpos2;
  auto l = this->currentLine;

  i = this->pos;
  if (i + delta < l->len()) {
    this->pos = i + delta;
  } else if (l->len() == 0) {
    this->pos = 0;
  } else {
    this->pos = l->len() - 1;
  }
  cpos = l->bytePosToColumn(this->pos);
  this->visualpos = l->width() + cpos - this->currentColumn;
  delta = 1;
  vpos2 = l->bytePosToColumn(this->pos + delta) - this->currentColumn - 1;
  if (vpos2 >= this->COLS && n) {
    this->columnSkip(n + (vpos2 - this->COLS) - (vpos2 - this->COLS) % n);
    this->visualpos = l->width() + cpos - this->currentColumn;
  }
  this->cursorX = this->visualpos - l->width();
}

void LineLayout::cursorLeft(int n) {
  if (empty())
    return;

  int i, delta = 1, cpos;
  auto l = this->currentLine;

  i = this->pos;
  if (i >= delta)
    this->pos = i - delta;
  else
    this->pos = 0;
  cpos = l->bytePosToColumn(this->pos);
  this->visualpos = l->width() + cpos - this->currentColumn;
  if (this->visualpos - l->width() < 0 && n) {
    this->columnSkip(-n + this->visualpos - l->width() -
                     (this->visualpos - l->width()) % n);
    this->visualpos = l->width() + cpos - this->currentColumn;
  }
  this->cursorX = this->visualpos - l->width();
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
  this->topLine = this->lineSkip(this->firstLine(), orig.TOP_LINENUMBER() - 1);
  this->gotoLine(orig.CUR_LINENUMBER());
  this->pos = orig.pos;
  this->currentColumn = orig.currentColumn;
  this->arrangeCursor();
}

/* go to the next downward/upward anchor */
void LineLayout::nextY(int d, int n) {
  if (empty()) {
    return;
  }

  auto hl = this->data._hmarklist;
  if (hl->size() == 0) {
    return;
  }

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = this->retrieveCurrentForm();
  }

  int x = this->pos;
  int y = linenumber(this->currentLine) + d;
  Anchor *pan = nullptr;
  int hseq = -1;
  for (int i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = nullptr;
    for (; y >= 0 && y <= linenumber(this->lastLine()); y += d) {
      if (this->data._href) {
        an = this->data._href->retrieveAnchor(y, x);
      }
      if (!an && this->data._formitem) {
        an = this->data._formitem->retrieveAnchor(y, x);
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
}

/* go to the next left/right anchor */
void LineLayout::nextX(int d, int dy, int n) {
  if (empty())
    return;

  auto hl = this->data._hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = retrieveCurrentForm();
  }

  auto l = this->currentLine;
  auto x = this->pos;
  auto y = linenumber(l);
  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = nullptr;
    while (1) {
      for (; x >= 0 && x < l->len(); x += d) {
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(y, x);
        }
        if (!an && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(y, x);
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
  this->gotoLine(y);
  this->pos = pan->start.pos;
  this->arrangeCursor();
}

/* go to the previous anchor */
void LineLayout::_prevA(std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->data._hmarklist;
  BufferPoint *po;
  Anchor *pan;
  int i, x, y;
  Url url;

  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  y = linenumber(this->currentLine);
  x = this->pos;

  for (i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq - 1;
      do {
        if (hseq < 0) {
          an = pan;
          goto _end;
        }
        po = &hl->marks[hseq];
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(po->line, po->pos);
        }
        if (an == nullptr && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq--;
      } while (an == nullptr || an == pan);
    } else {
      an = this->data._href->closest_prev_anchor(nullptr, x, y);
      an = this->data._formitem->closest_prev_anchor(an, x, y);
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
  po = &hl->marks[an->hseq];
  this->gotoLine(po->line);
  this->pos = po->pos;
  this->arrangeCursor();
}

/* go to the next anchor */
void LineLayout::_nextA(std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->data._hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  auto y = linenumber(currentLine);
  auto x = this->pos;

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
        auto po = &hl->marks[hseq];
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(po->line, po->pos);
        }
        if (an == nullptr && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq++;
      } while (an == nullptr || an == pan);
    } else {
      an = this->data._href->closest_next_anchor(nullptr, x, y);
      an = this->data._formitem->closest_next_anchor(an, x, y);
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
  auto po = &hl->marks[an->hseq];
  this->gotoLine(po->line);
  this->pos = po->pos;
  this->arrangeCursor();
}

/* Move cursor left */
void LineLayout::_movL(int n, int m) {
  if (empty())
    return;
  for (int i = 0; i < m; i++) {
    this->cursorLeft(n);
  }
}

/* Move cursor downward */
void LineLayout::_movD(int n, int m) {
  if (empty()) {
    return;
  }
  for (int i = 0; i < m; i++) {
    this->cursorDown(n);
  }
}

/* move cursor upward */
void LineLayout::_movU(int n, int m) {
  if (empty()) {
    return;
  }
  for (int i = 0; i < m; i++) {
    this->cursorUp(n);
  }
}

/* Move cursor right */
void LineLayout::_movR(int n, int m) {
  if (empty())
    return;
  for (int i = 0; i < m; i++) {
    this->cursorRight(n);
  }
}

int LineLayout::prev_nonnull_line(Line *line) {
  Line *l;
  for (l = line; !isNull(l) && l->len() == 0; --l)
    ;
  if (l == nullptr || l->len() == 0)
    return -1;

  this->currentLine = l;
  if (l != line)
    this->pos = this->currentLine->len();
  return 0;
}

int LineLayout::next_nonnull_line(Line *line) {
  Line *l;
  for (l = line; !isNull(l) && l->len() == 0; ++l)
    ;
  if (l == nullptr || l->len() == 0)
    return -1;

  this->currentLine = l;
  if (l != line)
    this->pos = 0;
  return 0;
}

/* Go to specified line */
void LineLayout::_goLine(const char *l, int prec_num) {
  if (l == nullptr || *l == '\0' || this->currentLine == nullptr) {
    return;
  }

  this->pos = 0;
  if (((*l == '^') || (*l == '$')) && prec_num) {
    this->gotoLine(prec_num);
  } else if (*l == '^') {
    this->topLine = this->currentLine = this->firstLine();
  } else if (*l == '$') {
    this->topLine = this->lineSkip(this->lastLine(), -(this->LINES + 1) / 2);
    this->currentLine = this->lastLine();
  } else
    this->gotoLine(atoi(l));
  this->arrangeCursor();
}

void LineLayout::shiftvisualpos(int shift) {
  Line *l = this->currentLine;
  this->visualpos -= shift;
  if (this->visualpos - l->width() >= this->COLS)
    this->visualpos = l->width() + this->COLS - 1;
  else if (this->visualpos - l->width() < 0)
    this->visualpos = l->width();
  this->arrangeLine();
  if (this->visualpos - l->width() == -shift && this->cursorX == 0)
    this->visualpos = l->width();
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
  while (e < l->len() && is_wordchar(p[e])) {
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
  if (empty()) {
    return;
  }

  auto top = this->topLine;
  auto cur = this->currentLine;

  auto lnum = linenumber(cur);
  this->topLine = this->lineSkip(top, n);
  if (this->topLine == top) {
    lnum += n;
    if (lnum < linenumber(topLine))
      lnum = linenumber(topLine);
    else if (lnum > linenumber(lastLine()))
      lnum = linenumber(lastLine());
  } else {
    auto tlnum = linenumber(topLine);
    auto llnum = linenumber(topLine) + this->LINES - 1;
    int diff_n;
    if (nextpage_topline)
      diff_n = 0;
    else
      diff_n = n - (tlnum - linenumber(top));
    if (lnum < tlnum)
      lnum = tlnum + diff_n;
    if (lnum > llnum)
      lnum = llnum + diff_n;
  }
  this->gotoLine(lnum);
  this->arrangeLine();
  if (n > 0) {
  } else {
    if (this->currentLine->width() + this->currentLine->width() <
        this->currentColumn + this->visualpos)
      this->cursorUp(1);
  }
  // App::instance().invalidate(mode);
}

void LineLayout::save_buffer_position() {
  if (empty())
    return;

  BufferPos *b = this->undo;
  if (b && b->top_linenumber == this->TOP_LINENUMBER() &&
      b->cur_linenumber == this->CUR_LINENUMBER() &&
      b->currentColumn == this->currentColumn && b->pos == this->pos)
    return;
  b = (BufferPos *)New(BufferPos);
  b->top_linenumber = this->TOP_LINENUMBER();
  b->cur_linenumber = this->CUR_LINENUMBER();
  b->currentColumn = this->currentColumn;
  b->pos = this->pos;
  b->next = NULL;
  b->prev = this->undo;
  if (this->undo)
    this->undo->next = b;
  this->undo = b;
}

void LineLayout::resetPos(BufferPos *b) {
  LineLayout buf;
  *buf.topLine = {};
  *buf.currentLine = {};
  buf.pos = b->pos;
  buf.currentColumn = b->currentColumn;
  this->restorePosition(buf);
  this->undo = b;
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

/* mark URL-like patterns as anchors */
static const char *url_like_pat[] = {
    "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
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
void LineLayout::chkURLBuffer() {
  for (int i = 0; url_like_pat[i]; i++) {
    this->data.reAnchor(topLine, url_like_pat[i]);
  }
  this->check_url = true;
}

void LineLayout::reshape(int width, const LineLayout &sbuf) {
  if (!this->data.need_reshape) {
    return;
  }
  this->data.need_reshape = false;
  this->width = width;

  this->height = App::instance().LASTLINE() + 1;
  if (!empty() && !sbuf.empty()) {
    Line *cur = sbuf.currentLine;
    this->pos = sbuf.pos;
    this->gotoLine(sbuf.linenumber(cur));
    int n = (linenumber(currentLine) - linenumber(topLine)) -
            (sbuf.linenumber(cur) - sbuf.linenumber(sbuf.topLine));
    if (n) {
      this->topLine = this->lineSkip(this->topLine, n);
      this->gotoLine(sbuf.linenumber(cur));
    }
    this->currentColumn = sbuf.currentColumn;
    this->arrangeCursor();
  }
  if (this->check_url) {
    this->chkURLBuffer();
  }
  formResetBuffer(this, sbuf.data._formitem.get());
}

Anchor *LineLayout::retrieveCurrentAnchor() {
  if (!this->currentLine || !this->data._href)
    return NULL;
  return this->data._href->retrieveAnchor(linenumber(currentLine), this->pos);
}

Anchor *LineLayout::retrieveCurrentImg() {
  if (!this->currentLine || !this->data._img)
    return NULL;
  return this->data._img->retrieveAnchor(linenumber(currentLine), this->pos);
}

Anchor *LineLayout::retrieveCurrentForm() {
  if (!this->currentLine || !this->data._formitem)
    return NULL;
  return this->data._formitem->retrieveAnchor(linenumber(currentLine),
                                              this->pos);
}
