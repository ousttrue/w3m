#include "line_layout.h"
#include "myctype.h"
#include "history.h"
#include "url.h"
#include "display.h"
#include "message.h"
#include "terms.h"
#include "anchor.h"
#include "form.h"

int nextpage_topline = false;

LineLayout::LineLayout() {
  this->COLS = ::COLS;
  this->LINES = LASTLINE;
}

LineLayout::LineLayout(int width) : width(width) {
  this->COLS = ::COLS;
  this->LINES = LASTLINE;
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
    set_delayed_message(msg);
    this->topLine = this->currentLine = l;
    return;
  }
  if (this->lastLine->linenumber < n) {
    l = this->lastLine;
    snprintf(msg, sizeof(msg), "Last line is #%ld", this->lastLine->linenumber);
    set_delayed_message(msg);
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
    set_delayed_message(msg);
    this->topLine = this->currentLine = l;
    return;
  }

  if (this->lastLine->real_linenumber < n) {
    l = this->lastLine;
    char msg[36];
    snprintf(msg, sizeof(msg), "Last line is #%ld",
             this->lastLine->real_linenumber);
    set_delayed_message(msg);
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

static AnchorList *putAnchor(AnchorList *al, const char *url,
                             const char *target, Anchor **anchor_return,
                             const char *referer, const char *title,
                             unsigned char key, int line, int pos) {
  if (al == NULL) {
    al = new AnchorList;
  }

  BufferPoint bp = {0};
  bp.line = line;
  bp.pos = pos;

  size_t n = al->size();
  size_t i;
  if (!n || bpcmp(al->anchors[n - 1].start, bp) < 0) {
    i = n;
  } else {
    for (i = 0; i < n; i++) {
      if (bpcmp(al->anchors[i].start, bp) >= 0) {
        for (size_t j = n; j > i; j--)
          al->anchors[j] = al->anchors[j - 1];
        break;
      }
    }
  }

  while (i >= al->anchors.size()) {
    al->anchors.push_back({});
  }
  auto a = &al->anchors[i];
  a->url = url;
  a->target = target;
  a->referer = referer;
  a->title = title;
  a->accesskey = key;
  a->slave = false;
  a->start = bp;
  a->end = bp;
  if (anchor_return)
    *anchor_return = a;
  return al;
}

Anchor *LineLayout::registerHref(const char *url, const char *target,
                                 const char *referer, const char *title,
                                 unsigned char key, int line, int pos) {
  Anchor *a;
  this->href =
      putAnchor(this->href, url, target, &a, referer, title, key, line, pos);
  return a;
}

Anchor *LineLayout::registerName(const char *url, int line, int pos) {
  Anchor *a;
  this->name =
      putAnchor(this->name, url, NULL, &a, NULL, NULL, '\0', line, pos);
  return a;
}

Anchor *LineLayout::registerImg(const char *url, const char *title, int line,
                                int pos) {
  Anchor *a;
  this->img = putAnchor(this->img, url, NULL, &a, NULL, title, '\0', line, pos);
  return a;
}

Anchor *LineLayout::registerForm(FormList *flist, HtmlTag *tag, int line,
                                 int pos) {

  auto fi = formList_addInput(flist, tag);
  if (!fi) {
    return NULL;
  }

  Anchor *a;
  this->formitem = putAnchor(this->formitem, (const char *)fi, flist->target,
                             &a, NULL, NULL, '\0', line, pos);
  return a;
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
        this->hmarklist->marks[a_form.hseq].line = l->linenumber;
        this->hmarklist->marks[a_form.hseq].pos = pos;
      }
      if (a_form.start.line == l->linenumber)
        continue;
      this->formitem = putAnchor(this->formitem, a_form.url, a_form.target, &a,
                                 NULL, NULL, '\0', l->linenumber, pos);
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

Anchor *LineLayout::searchURLLabel(const char *url) {
  return searchAnchor(this->name, url);
}

/* go to the next downward/upward anchor */
void LineLayout::nextY(int d, int n) {
  if (this->firstLine == nullptr)
    return;

  HmarkerList *hl = this->hmarklist;
  if (!hl || hl->nmark == 0)
    return;

  auto an = retrieveCurrentAnchor(this);
  if (an == nullptr)
    an = retrieveCurrentForm(this);

  int x = this->pos;
  int y = this->currentLine->linenumber + d;
  Anchor *pan = nullptr;
  int hseq = -1;
  for (int i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = nullptr;
    for (; y >= 0 && y <= this->lastLine->linenumber; y += d) {
      if (this->href) {
        an = this->href->retrieveAnchor(y, x);
      }
      if (!an && this->formitem) {
        an = this->formitem->retrieveAnchor(y, x);
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
  displayBuffer(B_NORMAL);
}

/* go to the next left/right anchor */
void LineLayout::nextX(int d, int dy, int n) {
  if (this->firstLine == nullptr)
    return;

  HmarkerList *hl = this->hmarklist;
  if (!hl || hl->nmark == 0)
    return;

  auto an = retrieveCurrentAnchor(this);
  if (an == nullptr)
    an = retrieveCurrentForm(this);

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
        if (this->href) {
          an = this->href->retrieveAnchor(y, x);
        }
        if (!an && this->formitem) {
          an = this->formitem->retrieveAnchor(y, x);
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
  displayBuffer(B_NORMAL);
}

/* go to the previous anchor */
void LineLayout::_prevA(int visited, std::optional<Url> baseUrl, int n) {
  HmarkerList *hl = this->hmarklist;
  BufferPoint *po;
  Anchor *pan;
  int i, x, y;
  Url url;

  if (this->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  auto an = retrieveCurrentAnchor(this);
  if (visited != true && an == nullptr)
    an = retrieveCurrentForm(this);

  y = this->currentLine->linenumber;
  x = this->pos;

  if (visited == true) {
    n = hl->nmark;
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
        po = hl->marks + hseq;
        if (this->href) {
          an = this->href->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && this->formitem) {
          an = this->formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq--;
        if (visited == true && an) {
          url = urlParse(an->url, baseUrl);
          if (getHashHist(URLHist, url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = closest_prev_anchor(this->href, nullptr, x, y);
      if (visited != true)
        an = closest_prev_anchor(this->formitem, an, x, y);
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
        if (getHashHist(URLHist, url.to_Str().c_str())) {
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
  po = hl->marks + an->hseq;
  this->gotoLine(po->line);
  this->pos = po->pos;
  this->arrangeCursor();
  displayBuffer(B_NORMAL);
}

/* go to the next [visited] anchor */
void LineLayout::_nextA(int visited, std::optional<Url> baseUrl, int n) {
  if (this->firstLine == nullptr)
    return;

  HmarkerList *hl = this->hmarklist;
  if (!hl || hl->nmark == 0)
    return;

  auto an = retrieveCurrentAnchor(this);
  if (visited != true && an == nullptr)
    an = retrieveCurrentForm(this);

  auto y = this->currentLine->linenumber;
  auto x = this->pos;

  if (visited == true) {
    n = hl->nmark;
  }

  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq + 1;
      do {
        if (hseq >= hl->nmark) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        auto po = &hl->marks[hseq];
        if (this->href) {
          an = this->href->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && this->formitem) {
          an = this->formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq++;
        if (visited == true && an) {
          auto url = urlParse(an->url, baseUrl);
          if (getHashHist(URLHist, url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = closest_next_anchor(this->href, nullptr, x, y);
      if (visited != true)
        an = closest_next_anchor(this->formitem, an, x, y);
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
        if (getHashHist(URLHist, url.to_Str().c_str())) {
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
  displayBuffer(B_NORMAL);
}

/* Move cursor left */
void LineLayout::_movL(int n, int m) {
  if (this->firstLine == nullptr)
    return;
  for (int i = 0; i < m; i++) {
    this->cursorLeft(n);
  }
  displayBuffer(B_NORMAL);
}

/* Move cursor downward */
void LineLayout::_movD(int n, int m) {
  if (this->firstLine == nullptr) {
    return;
  }
  for (int i = 0; i < m; i++) {
    this->cursorDown(n);
  }
  displayBuffer(B_NORMAL);
}

/* move cursor upward */
void LineLayout::_movU(int n, int m) {
  if (this->firstLine == nullptr) {
    return;
  }
  for (int i = 0; i < m; i++) {
    this->cursorUp(n);
  }
  displayBuffer(B_NORMAL);
}

/* Move cursor right */
void LineLayout::_movR(int n, int m) {
  if (this->firstLine == nullptr)
    return;
  for (int i = 0; i < m; i++) {
    this->cursorRight(n);
  }
  displayBuffer(B_NORMAL);
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
    displayBuffer(B_FORCE_REDRAW);
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
  displayBuffer(B_FORCE_REDRAW);
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
  // displayBuffer(mode);
}
