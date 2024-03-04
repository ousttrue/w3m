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

LineData::LineData() {
  this->_href = std::make_shared<AnchorList>();
  this->_name = std::make_shared<AnchorList>();
  this->_img = std::make_shared<AnchorList>();
  this->_formitem = std::make_shared<AnchorList>();
  this->_hmarklist = std::make_shared<HmarkerList>();
  this->_imarklist = std::make_shared<HmarkerList>();
}

void LineData::addnewline(const char *line, Lineprop *prop, int byteLen) {
  lines.push_back({line, prop, byteLen});
}

Anchor *LineData::registerForm(HtmlParser *parser, FormList *flist,
                               HtmlTag *tag, int line, int pos) {
  auto fi = flist->formList_addInput(parser, tag);
  if (!fi) {
    return NULL;
  }
  return this->formitem()->putAnchor((const char *)fi, flist->target, {}, NULL,
                                     '\0', line, pos);
}

void LineData::addMultirowsForm(AnchorList *al) {
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
    for (l = firstLine(); l != NULL; ++l) {
      if (linenumber(l) == a_form.y)
        break;
    }
    if (!l)
      continue;
    if (a_form.y == a_form.start.line)
      ls = l;
    else {
      for (ls = l; ls != NULL; (a_form.y < a_form.start.line) ? ++ls : --ls) {
        if (linenumber(ls) == a_form.start.line)
          break;
      }
      if (!ls)
        continue;
    }
    col = ls->bytePosToColumn(a_form.start.pos);
    ecol = ls->bytePosToColumn(a_form.end.pos);
    for (j = 0; l && j < a_form.rows; ++l, j++) {
      pos = l->columnPos(col);
      if (j == 0) {
        this->hmarklist()->marks[a_form.hseq].line = linenumber(l);
        this->hmarklist()->marks[a_form.hseq].pos = pos;
      }
      if (a_form.start.line == linenumber(l))
        continue;
      a = this->formitem()->putAnchor(a_form.url, a_form.target.c_str(), {},
                                      NULL, '\0', linenumber(l), pos);
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

/* renumber anchor */
void LineData::reseq_anchor() {
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

const char *LineData ::reAnchorPos(
    Line *l, const char *p1, const char *p2,
    Anchor *(*anchorproc)(LineData *, const char *, const char *, int, int)) {
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

  while (1) {
    a = anchorproc(this, p1, p2, linenumber(l), spos);
    a->hseq = hseq;
    if (hseq == -2) {
      this->reseq_anchor();
      hseq = a->hseq;
    }
    a->end.line = linenumber(l);
    a->end.pos = epos;
    break;
  }
  return p2;
}

/* search regexp and register them as anchors */
/* returns error message if any               */
const char *LineData::reAnchorAny(
    Line *topLine, const char *re,
    Anchor *(*anchorproc)(LineData *, const char *, const char *, int, int)) {
  Line *l;
  const char *p = NULL, *p1, *p2;

  if (re == NULL || *re == '\0') {
    return NULL;
  }
  if ((re = regexCompile(re, 1)) != NULL) {
    return re;
  }
  for (l = MarkAllPages ? this->firstLine() : topLine;
       l != NULL &&
       (MarkAllPages ||
        linenumber(l) < linenumber(topLine) + App::instance().LASTLINE());
       ++l) {
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

static Anchor *_put_anchor_all(LineData *layout, const char *p1, const char *p2,
                               int line, int pos) {
  auto tmp = Strnew_charp_n(p1, p2 - p1);
  return layout->href()->putAnchor(url_quote(tmp->ptr).c_str(), NULL,
                                   {.no_referer = true}, NULL, '\0', line, pos);
}

const char *LineData::reAnchor(Line *topLine, const char *re) {
  return this->reAnchorAny(topLine, re, _put_anchor_all);
}

const char *LineData::reAnchorWord(Line *l, int spos, int epos) {
  return this->reAnchorPos(l, &l->lineBuf[spos], &l->lineBuf[epos],
                           _put_anchor_all);
}

const char *LineData::getAnchorText(AnchorList *al, Anchor *a) {
  if (!a || a->hseq < 0)
    return NULL;

  Str *tmp = NULL;
  auto hseq = a->hseq;
  auto l = this->lines.begin();
  for (size_t i = 0; i < al->size(); i++) {
    a = &al->anchors[i];
    if (a->hseq != hseq)
      continue;
    for (; l != lines.end(); ++l) {
      if (linenumber(&*l) == a->start.line)
        break;
    }
    if (l == lines.end())
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

void LineData::addLink(struct HtmlTag *tag) {
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

//
// LineLayout
//
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

  auto hl = this->data.hmarklist();
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
      if (this->data.href()) {
        an = this->data.href()->retrieveAnchor(y, x);
      }
      if (!an && this->data.formitem()) {
        an = this->data.formitem()->retrieveAnchor(y, x);
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

  auto hl = this->data.hmarklist();
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
        if (this->data.href()) {
          an = this->data.href()->retrieveAnchor(y, x);
        }
        if (!an && this->data.formitem()) {
          an = this->data.formitem()->retrieveAnchor(y, x);
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
void LineLayout::_prevA(bool visited, std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->data.hmarklist();
  BufferPoint *po;
  Anchor *pan;
  int i, x, y;
  Url url;

  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (visited != true && an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  y = linenumber(this->currentLine);
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
        if (this->data.href()) {
          an = this->data.href()->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && this->data.formitem()) {
          an = this->data.formitem()->retrieveAnchor(po->line, po->pos);
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
      an = this->data.href()->closest_prev_anchor(nullptr, x, y);
      if (visited != true)
        an = this->data.formitem()->closest_prev_anchor(an, x, y);
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
}

/* go to the next [visited] anchor */
void LineLayout::_nextA(bool visited, std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->data.hmarklist();
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (visited != true && an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  auto y = linenumber(currentLine);
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
        if (this->data.href()) {
          an = this->data.href()->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && this->data.formitem()) {
          an = this->data.formitem()->retrieveAnchor(po->line, po->pos);
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
      an = this->data.href()->closest_next_anchor(nullptr, x, y);
      if (visited != true)
        an = this->data.formitem()->closest_next_anchor(an, x, y);
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
  formResetBuffer(this, sbuf.data.formitem().get());
}
