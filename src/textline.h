#pragma once
#include "generallist.h"
#include "Str.h"

struct TextLine {
  struct Str *line;
  int pos;
};

extern TextLine *newTextLine(Str *line, int pos);

inline TextLine *newTextLine(const char *line) {
  return newTextLine(Strnew_charp(line ? line : ""), 0);
}

extern void appendTextLine(GeneralList<TextLine> *tl, Str *line, int pos);

enum AlignMode {
  ALIGN_CENTER = 0,
  ALIGN_LEFT = 1,
  ALIGN_RIGHT = 2,
  ALIGN_MIDDLE = 4,
  ALIGN_TOP = 5,
  ALIGN_BOTTOM = 6,
};

bool toAlign(char *oval, AlignMode *align);
void align(TextLine *lbuf, int width, AlignMode mode);
