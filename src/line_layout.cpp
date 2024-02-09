#include "line_layout.h"
#include "message.h"
#include "terms.h"

int nextpage_topline = false;

LineLayout::LineLayout() {
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
    sprintf(msg, "First line is #%ld", l->linenumber);
    set_delayed_message(msg);
    this->topLine = this->currentLine = l;
    return;
  }
  if (this->lastLine->linenumber < n) {
    l = this->lastLine;
    sprintf(msg, "Last line is #%ld", this->lastLine->linenumber);
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
