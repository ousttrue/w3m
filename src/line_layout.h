#pragma once
#include "line.h"

struct Line;
struct LineLayout {
  LineLayout();

  //
  // lines
  //
  Line *firstLine = nullptr;
  Line *lastLine = nullptr;
  // scroll position
  Line *topLine = nullptr;
  int currentColumn = 0;
  // cursor position
  Line *currentLine = nullptr;

  int TOP_LINENUMBER() const { return topLine ? topLine->linenumber : 1; }
  int CUR_LINENUMBER() const {
    return currentLine ? currentLine->linenumber : 1;
  }
  void pushLine(Line *l) {
    if (!this->lastLine || this->lastLine == this->currentLine) {
      this->lastLine = l;
    }
    this->currentLine = l;
    if (!this->firstLine) {
      this->firstLine = l;
    }
  }

  //
  // viewport
  //
  short rootX = 0;
  short rootY = 0;
  short COLS = 0;
  short LINES = 0;
  short cursorX = 0;
  short cursorY = 0;

  int AbsCursorX() const { return cursorX + rootX; }
  int AbsCursorY() const { return cursorY + rootY; }

  inline void COPY_BUFROOT_FROM(const LineLayout &srcbuf) {
    this->rootX = srcbuf.rootX;
    this->rootY = srcbuf.rootY;
    this->COLS = srcbuf.COLS;
    this->LINES = srcbuf.LINES;
  }
};
