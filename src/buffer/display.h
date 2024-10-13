#pragma once
#include "line.h"

extern bool displayLineInfo;
extern bool displayLink;
extern int enable_inline_image;
extern int nTab;

/* Flags for displayBuffer() */
enum DisplayMode {
  B_NORMAL = 0,
  B_FORCE_REDRAW = 1,
  B_REDRAW = 2,
  B_SCROLL = 3,
  B_REDRAW_IMAGE = 4,
};

struct Buffer;
void displayBuffer(struct Buffer *buf, enum DisplayMode mode);

void addChar(char c, Lineprop mode);
struct Document *message_list_panel();

void displayInvalidate();
