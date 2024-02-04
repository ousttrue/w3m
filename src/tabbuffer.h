#pragma once

struct Buffer;
struct TabBuffer {
  TabBuffer *nextTab;
  TabBuffer *prevTab;
  Buffer *currentBuffer;
  Buffer *firstBuffer;
  short x1;
  short x2;
  short y;
  void deleteBuffer(Buffer *delbuf);
};
#define NO_TABBUFFER ((TabBuffer *)1)

extern TabBuffer *CurrentTab;
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)

extern TabBuffer *FirstTab;
extern TabBuffer *LastTab;
extern bool open_tab_blank;
extern bool open_tab_dl_list;
extern bool close_tab_back;
extern int nTab;
extern int TabCols;

TabBuffer *newTab();
void calcTabPos();
TabBuffer *deleteTab(TabBuffer *tab);
TabBuffer *numTab(int n);
