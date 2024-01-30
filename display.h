#pragma once

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

void message(char *s, int return_x, int return_y);
