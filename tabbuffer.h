#pragma once

struct TabBuffer {
  struct TabBuffer *nextTab;
  struct TabBuffer *prevTab;
  struct Buffer *currentBuffer;
  struct Buffer *firstBuffer;
  short x1;
  short x2;
  short y;
};

#define NO_TABBUFFER ((struct TabBuffer *)1)
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)

extern struct TabBuffer *CurrentTab;
extern struct TabBuffer *FirstTab;
extern struct TabBuffer *LastTab;

extern struct TabBuffer *newTab();
extern void calcTabPos();
extern struct TabBuffer *deleteTab(struct TabBuffer *tab);
