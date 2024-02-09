#include "line_layout.h"
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
