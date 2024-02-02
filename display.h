#pragma once
#include "line.h"
#include <stddef.h>

extern bool displayLink;
extern bool enable_inline_image;

struct Buffer;

enum DisplayFlag {
  B_NORMAL,
  B_FORCE_REDRAW,
  B_REDRAW,
  B_SCROLL,
  B_REDRAW_IMAGE,
};

void displayBuffer(Buffer *buf, DisplayFlag mode);

void fmInit(void);
void fmTerm(void);

void addChar(char c, Lineprop mode);
void addMChar(char *p, Lineprop mode, size_t len);
void restorePosition(Buffer *buf, Buffer *orig);
