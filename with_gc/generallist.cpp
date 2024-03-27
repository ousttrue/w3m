#include "generallist.h"
#include "cmp.h"

bool toAlign(char *oval, AlignMode *align) {
  if (strcasecmp(oval, "left") == 0)
    *align = ALIGN_LEFT;
  else if (strcasecmp(oval, "right") == 0)
    *align = ALIGN_RIGHT;
  else if (strcasecmp(oval, "center") == 0)
    *align = ALIGN_CENTER;
  else if (strcasecmp(oval, "top") == 0)
    *align = ALIGN_TOP;
  else if (strcasecmp(oval, "bottom") == 0)
    *align = ALIGN_BOTTOM;
  else if (strcasecmp(oval, "middle") == 0)
    *align = ALIGN_MIDDLE;
  else
    return false;
  return true;
}

void appendTextLine(GeneralList *tl, Str *line, int pos) {
  if (tl->last == NULL) {
    tl->pushValue(TextLine::newTextLine(line->Strdup(), pos));
  } else {
    auto lbuf = tl->last->ptr;
    if (lbuf->line)
      Strcat(lbuf->line, line);
    else
      lbuf->line = line;
    lbuf->_pos += pos;
  }
}

TextLine *TextLine::newTextLine(Str *line, int pos) {
  auto lbuf = (TextLine *)New(TextLine);
  if (line)
    lbuf->line = line;
  else
    lbuf->line = Strnew();
  lbuf->_pos = pos;
  return lbuf;
}

TextLine *TextLine::newTextLine(const char *line) {
  return newTextLine(Strnew_charp(line ? line : ""), 0);
}

void TextLine::align(int width, AlignMode mode) {
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

GeneralList *GeneralList::newGeneralList() {
  auto tl = (GeneralList *)New(GeneralList);
  tl->first = tl->last = NULL;
  tl->nitem = 0;
  return tl;
}

void GeneralList::pushValue(TextLine *s) {
  ListItem *it;
  if (s == NULL /*|| tl == NULL*/ || this->nitem >= GENERAL_LIST_MAX)
    return;
  it = ListItem::newListItem(s, NULL, this->last);
  if (this->first == NULL) {
    this->first = it;
    this->last = it;
    this->nitem = 1;
  } else {
    this->last->next = it;
    this->last = it;
    this->nitem++;
  }
}

TextLine *GeneralList::popValue() {
  if (!this->first) {
    return nullptr;
  }
  auto f = this->first;
  this->first = f->next;
  if (this->first)
    this->first->prev = NULL;
  else
    this->last = NULL;
  this->nitem--;
  return f->ptr;
}

TextLine *GeneralList::rpopValue() {
  if (this->last == NULL)
    return NULL;
  auto f = this->last;
  this->last = f->prev;
  if (this->last)
    this->last->next = NULL;
  else
    this->first = NULL;
  this->nitem--;
  return f->ptr;
}

void GeneralList::delValue(ListItem *it) {
  if (it->prev)
    it->prev->next = it->next;
  else
    this->first = it->next;
  if (it->next)
    it->next->prev = it->prev;
  else
    this->last = it->prev;
  this->nitem--;
}

void GeneralList::appendGeneralList(GeneralList *tl2) {
  if (/*tl &&*/ tl2) {
    if (tl2->first) {
      if (this->last) {
        if (this->nitem + tl2->nitem > GENERAL_LIST_MAX) {
          return;
        }
        this->last->next = tl2->first;
        tl2->first->prev = this->last;
        this->last = tl2->last;
        this->nitem += tl2->nitem;
      } else {
        this->first = tl2->first;
        this->last = tl2->last;
        this->nitem = tl2->nitem;
      }
    }
    tl2->first = tl2->last = NULL;
    tl2->nitem = 0;
  }
}
