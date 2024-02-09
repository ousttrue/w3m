#include "line_layout.h"
#include "terms.h"

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
