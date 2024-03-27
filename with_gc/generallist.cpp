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
