#pragma once

extern bool clear_buffer;

extern int nTab;
extern int TabCols;

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

void tabInitialize(struct Buffer *newbuf);
void _newT(struct Buffer *buf);
struct TabBuffer *numTab(int n);
void calcTabPos();
struct TabBuffer *deleteTab(struct TabBuffer *tab);
void pushBuffer(struct TabBuffer *tab, struct Buffer *buf);
struct FormList;
bool handleMailto(const char *url);
void moveTab(struct TabBuffer *t, struct TabBuffer *t2, int right);
