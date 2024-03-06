#pragma once
#include "generallist.h"
#include "Str.h"

enum AlignMode {
  ALIGN_CENTER = 0,
  ALIGN_LEFT = 1,
  ALIGN_RIGHT = 2,
  ALIGN_MIDDLE = 4,
  ALIGN_TOP = 5,
  ALIGN_BOTTOM = 6,
};
bool toAlign(char *oval, AlignMode *align);

struct TextLine {
  friend void appendTextLine(GeneralList<TextLine> *tl, Str *line, int pos);

  struct Str *line;

private:
  int _pos;

public:
  int pos() const { return _pos; }
  static TextLine *newTextLine(Str *line, int pos) {
    auto lbuf = (TextLine *)New(TextLine);
    if (line)
      lbuf->line = line;
    else
      lbuf->line = Strnew();
    lbuf->_pos = pos;
    return lbuf;
  }

  static TextLine *newTextLine(const char *line) {
    return newTextLine(Strnew_charp(line ? line : ""), 0);
  }

  void align(int width, AlignMode mode) {
    Str *line = this->line;
    if (line->length == 0) {
      for (int i = 0; i < width; i++)
        Strcat_char(line, ' ');
      this->_pos = width;
      return;
    }

    auto buf = Strnew();
    int l = width - this->_pos;
    switch (mode) {
    case ALIGN_CENTER: {
      int l1 = l / 2;
      int l2 = l - l1;
      for (int i = 0; i < l1; i++)
        Strcat_char(buf, ' ');
      Strcat(buf, line);
      for (int i = 0; i < l2; i++)
        Strcat_char(buf, ' ');
      break;
    }
    case ALIGN_LEFT:
      Strcat(buf, line);
      for (int i = 0; i < l; i++)
        Strcat_char(buf, ' ');
      break;
    case ALIGN_RIGHT:
      for (int i = 0; i < l; i++)
        Strcat_char(buf, ' ');
      Strcat(buf, line);
      break;
    default:
      return;
    }
    this->line = buf;
    if (this->_pos < width)
      this->_pos = width;
  }
};

extern void appendTextLine(GeneralList<TextLine> *tl, Str *line, int pos);
