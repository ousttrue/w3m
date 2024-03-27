#include "textline.h"
#include "cmp.h"
#include "alloc.h"
#include "Str.h"
#include "generallist.h"

void appendTextLine(GeneralList<TextLine> *tl, Str *line, int pos) {
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
