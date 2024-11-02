#pragma once
#include "term/termsize.h"

extern bool clear_buffer;
extern bool close_tab_back;
extern bool open_tab_blank;

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

struct Content;
void tabInitialize(struct Content *content);
void _newT(struct Content *doc);
struct TabBuffer *numTab(int n);
void calcTabPos(struct TermSize size);
struct TabBuffer *deleteTab(struct TabBuffer *tab);
void pushContent(struct TabBuffer *tab, struct Content *content);
void pushCheckTarget(struct TabBuffer *tab, const char *anchor_target,
                     struct Content *content, bool check_target);
struct FormList;
bool handleMailto(const char *url);
void moveTab(struct TabBuffer *t, struct TabBuffer *t2, int right);
void _backBf(struct TabBuffer *tab);
void delBuffer(struct TabBuffer *tab, struct Buffer *buf);
