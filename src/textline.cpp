#include "textline.h"
#include "quote.h"
#include "alloc.h"
#include "Str.h"
#include "textlist.h"

TextLine *newTextLine(Str *line, int pos) {
  auto lbuf = (TextLine *)New(TextLine);
  if (line)
    lbuf->line = line;
  else
    lbuf->line = Strnew();
  lbuf->pos = pos;
  return lbuf;
}

void appendTextLine(TextLineList *tl, Str *line, int pos) {
  TextLine *lbuf;

  if (tl->last == NULL) {
    pushTextLine(tl, newTextLine(line->Strdup(), pos));
  } else {
    lbuf = tl->last->ptr;
    if (lbuf->line)
      Strcat(lbuf->line, line);
    else
      lbuf->line = line;
    lbuf->pos += pos;
  }
}

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

void align(TextLine *lbuf, int width, AlignMode mode) {
  Str *line = lbuf->line;
  if (line->length == 0) {
    for (int i = 0; i < width; i++)
      Strcat_char(line, ' ');
    lbuf->pos = width;
    return;
  }
  auto buf = Strnew();
  int l = width - lbuf->pos;
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
  lbuf->line = buf;
  if (lbuf->pos < width)
    lbuf->pos = width;
}

void pushTextLine(TextLineList *tl, TextLine *lbuf) {
  pushValue((GeneralList *)(tl), (void *)(lbuf));
}

#define popTextLine(tl) ((TextLine *)popValue((GeneralList *)(tl)))

TextLine *rpopTextLine(TextLineList *tl) {
  return ((TextLine *)rpopValue((GeneralList *)(tl)));
}
