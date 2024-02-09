#pragma once
#include "line.h"

struct Line;
struct LineLayout {
  Line *firstLine = nullptr;
  Line *topLine = nullptr;
  Line *currentLine = nullptr;
  Line *lastLine = nullptr;

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
};
