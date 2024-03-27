#pragma once
#include "alloc.h"
#include "Str.h"

#define GENERAL_LIST_MAX (INT_MAX / 32)
enum AlignMode {
  ALIGN_CENTER = 0,
  ALIGN_LEFT = 1,
  ALIGN_RIGHT = 2,
  ALIGN_MIDDLE = 4,
  ALIGN_TOP = 5,
  ALIGN_BOTTOM = 6,
};
bool toAlign(char *oval, AlignMode *align);

struct GeneralList;
struct TextLine {
  friend void appendTextLine(GeneralList *tl, Str *line, int pos);

  struct Str *line;

private:
  int _pos;

public:
  int pos() const { return _pos; }
  static TextLine *newTextLine(Str *line, int pos);
  static TextLine *newTextLine(const char *line);
  void align(int width, AlignMode mode);
};

extern void appendTextLine(GeneralList *tl, Str *line, int pos);
struct ListItem {
  struct TextLine *ptr;
  ListItem *next;
  ListItem *prev;
  static ListItem *newListItem(struct TextLine *s, ListItem *n, ListItem *p) {
    auto it = (ListItem *)New(ListItem);
    it->ptr = s;
    it->next = n;
    it->prev = p;
    return it;
  }
};

struct GeneralList {
  ListItem *first;
  ListItem *last;
  int nitem;

  static GeneralList *newGeneralList();
  void pushValue(TextLine *s);
  TextLine *popValue();
  TextLine *rpopValue();
  void delValue(ListItem *it);
  void appendGeneralList(GeneralList *tl2);
};
