#pragma once

struct TextLine {
  struct Str *line;
  int pos;
};

struct TextLineListItem {
  TextLine *ptr;
  TextLineListItem *next;
  TextLineListItem *prev;
};

struct TextLineList {
  TextLineListItem *first;
  TextLineListItem *last;
  int nitem;
};

extern TextLine *newTextLine(Str *line, int pos);
extern void appendTextLine(TextLineList *tl, Str *line, int pos);

TextLineList *newTextLineList();

void pushTextLine(TextLineList *tl, TextLine *lbuf);

#define popTextLine(tl) ((TextLine *)popValue((GeneralList *)(tl)))

TextLine *rpopTextLine(TextLineList *tl);

#define appendTextLineList(tl, tl2)                                            \
  ((TextLineList *)appendGeneralList((GeneralList *)(tl), (GeneralList *)(tl2)))

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
