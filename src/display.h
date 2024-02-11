#pragma once
#include "lineprop.h"
#include <stddef.h>
#include <memory>

extern bool displayLink;
extern bool showLineNum;
int _INIT_BUFFER_WIDTH();
int INIT_BUFFER_WIDTH();
extern bool FoldLine;
int FOLD_BUFFER_WIDTH();

enum DisplayFlag {
  B_NORMAL,
  B_FORCE_REDRAW,
  B_REDRAW,
  B_SCROLL,
  B_REDRAW_IMAGE,
};

struct Buffer;
void displayBuffer(DisplayFlag mode);

void fmInit(void);
void fmTerm(void);

void addChar(char c, Lineprop mode);
void addMChar(char *p, Lineprop mode, size_t len);
void nscroll(int n, DisplayFlag mode);
