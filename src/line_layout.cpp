#include "line_layout.h"
#include "app.h"
#include "myctype.h"
#include "url.h"
#include "html/form.h"
#include "html/form_item.h"
#include "regex.h"
#include "alloc.h"
#include "utf8.h"

int nextpage_topline = false;
int FoldTextarea = false;

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
  this->cursor.row = linenumber(currentLine) - linenumber(topLine);
  auto i = this->currentLine->columnPos(this->currentColumn + this->visualpos -
                                        this->currentLine->width());
  auto cpos = this->currentLine->bytePosToColumn(i) - this->currentColumn;
  if (cpos >= 0) {
    this->cursor.col = cpos;
    this->pos = i;
  } else if (this->currentLine->len() > i) {
    this->cursor.col = 0;
    this->pos = i + 1;
  } else {
    this->cursor.col = 0;
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
  if (this->cursor.row > 0)
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
  if (this->cursor.row < this->LINES - 1)
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
  this->cursor.row = linenumber(currentLine) - linenumber(topLine);
  this->visualpos = this->currentLine->width() +
                    this->currentLine->bytePosToColumn(this->pos) -
                    this->currentColumn;
  this->cursor.col = this->visualpos - this->currentLine->width();
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
  this->cursor.col = this->visualpos - l->width();
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
  this->cursor.col = this->visualpos - l->width();
}

void LineLayout::cursorHome() {
  this->visualpos = 0;
  this->cursor = {0, 0};
}

void LineLayout::cursorXY(int x, int y) {
  this->cursorUpDown(y - this->cursor.row);

  if (this->cursor.col > x) {
    while (this->cursor.col > x) {
      this->cursorLeft(this->COLS / 2);
    }
  } else if (this->cursor.col < x) {
    while (this->cursor.col < x) {
      auto oldX = this->cursor.col;

      this->cursorRight(this->COLS / 2);

      if (oldX == this->cursor.col) {
        break;
      }
    }
    if (this->cursor.col > x) {
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
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  int y = linenumber(this->currentLine);
  int x = this->pos;

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
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(po->line, po->pos);
        }
        if (an == nullptr && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq--;
      } while (an == nullptr || an == pan);
    } else {
      an = this->data._href->closest_prev_anchor((Anchor *)nullptr, x, y);
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

  {
    auto po = hl->get(an->hseq);
    this->gotoLine(po->line);
    this->pos = po->pos;
    this->arrangeCursor();
  }
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
        auto po = hl->get(hseq);
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(po->line, po->pos);
        }
        if (an == nullptr && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq++;
      } while (an == nullptr || an == pan);
    } else {
      an = this->data._href->closest_next_anchor((Anchor *)nullptr, x, y);
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
  {
    auto po = hl->get(an->hseq);
    this->gotoLine(po->line);
    this->pos = po->pos;
    this->arrangeCursor();
  }
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
  if (this->visualpos - l->width() == -shift && this->cursor.col == 0)
    this->visualpos = l->width();
}

std::string LineLayout::getCurWord(int *spos, int *epos) const {
  Line *l = this->currentLine;
  if (l == nullptr)
    return {};

  *spos = 0;
  *epos = 0;
  auto p = l->lineBuf();
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
  return {p + b, p + e};
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
  this->formResetBuffer(sbuf.data._formitem.get());
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

FormAnchor *LineLayout::retrieveCurrentForm() {
  if (!this->currentLine || !this->data._formitem)
    return NULL;
  return this->data._formitem->retrieveAnchor(linenumber(currentLine),
                                              this->pos);
}

void LineLayout::formResetBuffer(const AnchorList<FormAnchor> *formitem) {

  if (this->data._formitem == NULL || formitem == NULL)
    return;

  for (size_t i = 0; i < this->data._formitem->size() && i < formitem->size();
       i++) {
    auto a = &this->data._formitem->anchors[i];
    if (a->y != a->start.line)
      continue;

    auto f1 = a->formItem;
    auto f2 = formitem->anchors[i].formItem;
    if (f1->type != f2->type ||
        strcmp(((f1->name == NULL) ? "" : f1->name->ptr),
               ((f2->name == NULL) ? "" : f2->name->ptr)))
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

static int form_update_line(Line *line, char **str, int spos, int epos,
                            int width, int newline, int password) {
  int c_len = 1, c_width = 1, w, i, len, pos;
  char *p, *buf;
  Lineprop c_type, effect, *prop;

  for (p = *str, w = 0, pos = 0; *p && w < width;) {
    c_type = get_mctype(p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        w++;
        pos++;
      }
    } else if (password) {
      w += c_width;
      pos += c_width;
      w += c_width;
      pos += c_len;
    }
    p += c_len;
  }
  pos += width - w;

  len = line->len() + pos + spos - epos;
  buf = (char *)New_N(char, len + 1);
  buf[len] = '\0';
  prop = (Lineprop *)New_N(Lineprop, len);
  memcpy(buf, line->lineBuf(), spos * sizeof(char));
  memcpy(prop, line->propBuf(), spos * sizeof(Lineprop));

  effect = CharEffect(line->propBuf()[spos]);
  for (p = *str, w = 0, pos = spos; *p && w < width;) {
    c_type = get_mctype(p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        buf[pos] = password ? '*' : ' ';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else if (password) {
      for (i = 0; i < c_width; i++) {
        buf[pos] = '*';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else {
      buf[pos] = *p;
      prop[pos] = effect | c_type;
      pos++;
      w += c_width;
    }
    p += c_len;
  }
  for (; w < width; w++) {
    buf[pos] = ' ';
    prop[pos] = effect | PC_ASCII;
    pos++;
  }
  if (newline) {
    if (!FoldTextarea) {
      while (*p && *p != '\r' && *p != '\n')
        p++;
    }
    if (*p == '\r')
      p++;
    if (*p == '\n')
      p++;
  }
  *str = p;

  memcpy(&buf[pos], &line->lineBuf()[epos],
         (line->len() - epos) * sizeof(char));
  memcpy(&prop[pos], &line->propBuf()[epos],
         (line->len() - epos) * sizeof(Lineprop));

  line->assign(buf, prop, len);

  return pos;
}
void LineLayout::formUpdateBuffer(FormAnchor *a) {
  char *p;
  int spos, epos, rows, c_rows, pos, col = 0;
  Line *l;

  LineLayout save = *this;
  this->gotoLine(a->start.line);
  auto form = a->formItem;
  switch (form->type) {
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

  switch (form->type) {
  case FORM_INPUT_CHECKBOX:
  case FORM_INPUT_RADIO:
    if (this->currentLine == NULL || spos >= this->currentLine->len() ||
        spos < 0)
      break;
    if (form->checked)
      this->currentLine->lineBuf(spos, '*');
    else
      this->currentLine->lineBuf(spos, ' ');
    break;
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_PASSWORD:
  case FORM_TEXTAREA: {
    if (!form->value)
      break;
    p = form->value->ptr;

    l = this->currentLine;
    if (!l)
      break;
    if (form->type == FORM_TEXTAREA) {
      int n = a->y - this->linenumber(this->currentLine);
      if (n > 0)
        for (; !this->isNull(l) && n; --l, n--)
          ;
      else if (n < 0)
        for (; !this->isNull(l) && n; --l, n++)
          ;
      if (!l)
        break;
    }
    rows = form->rows ? form->rows : 1;
    col = l->bytePosToColumn(a->start.pos);
    for (c_rows = 0; c_rows < rows; c_rows++, ++l) {
      if (this->isNull(l))
        break;
      if (rows > 1 && this->data._formitem) {
        pos = l->columnPos(col);
        a = this->data._formitem->retrieveAnchor(this->linenumber(l), pos);
        if (a == NULL)
          break;
        spos = a->start.pos;
        epos = a->end.pos;
      }
      if (a->start.line != a->end.line || spos > epos || epos >= l->len() ||
          spos < 0 || epos < 0 || l->bytePosToColumn(epos) < col)
        break;
      pos = form_update_line(l, &p, spos, epos, l->bytePosToColumn(epos) - col,
                             rows > 1, form->type == FORM_INPUT_PASSWORD);
      if (pos != epos) {
        this->data._href->shiftAnchorPosition(this->data._hmarklist.get(),
                                              a->start.line, spos, pos - epos);
        this->data._name->shiftAnchorPosition(this->data._hmarklist.get(),
                                              a->start.line, spos, pos - epos);
        this->data._img->shiftAnchorPosition(this->data._hmarklist.get(),
                                             a->start.line, spos, pos - epos);
        this->data._formitem->shiftAnchorPosition(
            this->data._hmarklist.get(), a->start.line, spos, pos - epos);
      }
    }
    break;
  }

  default:
    break;
  }
  *this = save;
  this->arrangeLine();
}
