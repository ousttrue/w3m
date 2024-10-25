#include "tabbuffer.h"
#include "alloc.h"
#include "buffer.h"
#include "document.h"
#include "term/termsize.h"

int nTab = 0;
int TabCols = 10;

struct TabBuffer *newTab(void) {
  auto n = New(struct TabBuffer);
  n->nextTab = nullptr;
  n->currentBuffer = nullptr;
  n->firstBuffer = nullptr;
  return n;
}

void tabInitialize(struct Buffer *newbuf) {
  if (!CurrentTab) {
    FirstTab = LastTab = CurrentTab = newTab();
    nTab = 1;
    Firstbuf = Currentbuf = newbuf;
  } else {
    Currentbuf->nextBuffer = newbuf;
    Currentbuf = newbuf;
  }
}

void _newT() {
  auto tag = newTab();
  auto buf = newBuffer();
  copyBuffer(buf->document, Currentbuf->document);
  buf->nextBuffer = NULL;
  for (int i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = NULL;
  (*buf->clone)++;
  tag->firstBuffer = tag->currentBuffer = buf;

  tag->nextTab = CurrentTab->nextTab;
  tag->prevTab = CurrentTab;
  if (CurrentTab->nextTab)
    CurrentTab->nextTab->prevTab = tag;
  else
    LastTab = tag;
  CurrentTab->nextTab = tag;
  CurrentTab = tag;
  nTab++;
}

struct TabBuffer *numTab(int n) {
  if (n == 0)
    return CurrentTab;
  if (n == 1)
    return FirstTab;
  if (nTab <= 1)
    return nullptr;

  int i;
  struct TabBuffer *tab;
  for (tab = FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
    ;
  return tab;
}

void calcTabPos(void) {
  if (nTab <= 0)
    return;

  int lcol = 0;
  int rcol = 0;
  int n2, ny;
  int n1 = (COLS - rcol - lcol) / TabCols;
  if (n1 >= nTab) {
    n2 = 1;
    ny = 1;
  } else {
    if (n1 < 0)
      n1 = 0;
    n2 = COLS / TabCols;
    if (n2 == 0)
      n2 = 1;
    ny = (nTab - n1 - 1) / n2 + 2;
  }

  int na = n1 + n2 * (ny - 1);
  n1 -= (na - nTab) / ny;
  if (n1 < 0)
    n1 = 0;
  na = n1 + n2 * (ny - 1);

  auto tab = FirstTab;
  for (int iy = 0; iy < ny && tab; iy++) {
    int nx;
    int col;
    if (iy == 0) {
      nx = n1;
      col = COLS - rcol - lcol;
    } else {
      nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
      col = COLS;
    }
    for (int ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
      tab->x1 = col * ix / nx;
      tab->x2 = col * (ix + 1) / nx - 1;
      tab->y = iy;
      if (iy == 0) {
        tab->x1 += lcol;
        tab->x2 += lcol;
      }
    }
  }
}

struct TabBuffer *deleteTab(struct TabBuffer *tab) {
  if (nTab <= 1)
    return FirstTab;
  if (tab->prevTab) {
    if (tab->nextTab)
      tab->nextTab->prevTab = tab->prevTab;
    else
      LastTab = tab->prevTab;
    tab->prevTab->nextTab = tab->nextTab;
    if (tab == CurrentTab)
      CurrentTab = tab->prevTab;
  } else { /* tab == FirstTab */
    tab->nextTab->prevTab = nullptr;
    FirstTab = tab->nextTab;
    if (tab == CurrentTab)
      CurrentTab = tab->nextTab;
  }

  nTab--;
  auto buf = tab->firstBuffer;
  while (buf && buf != NO_BUFFER) {
    auto next = buf->nextBuffer;
    discardBuffer(buf);
    buf = next;
  }
  return FirstTab;
}
