#include "generallist.h"
#include "cmp.h"
#include <sstream>

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

void TextLine::align(int width, AlignMode mode) {
  if (line.size()) {
    for (int i = 0; i < width; i++)
      line += ' ';
    this->pos = width;
    return;
  }

  std::stringstream buf;
  int l = width - this->pos;
  switch (mode) {
  case ALIGN_CENTER: {
    int l1 = l / 2;
    int l2 = l - l1;
    for (int i = 0; i < l1; i++)
      buf << ' ';
    buf << line;
    for (int i = 0; i < l2; i++)
      buf << ' ';
    break;
  }
  case ALIGN_LEFT:
    buf << line;
    for (int i = 0; i < l; i++)
      buf << ' ';
    break;
  case ALIGN_RIGHT:
    for (int i = 0; i < l; i++)
      buf << ' ';
    buf << line;
    break;
  default:
    return;
  }
  this->line = buf.str();
  if (this->pos < width)
    this->pos = width;
}
