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
void SAVE_BUFPOSITION(Buffer *sbufp);
void RESTORE_BUFPOSITION(Buffer *sbufp);

extern TabBuffer *FirstTab;
extern TabBuffer *LastTab;
extern bool open_tab_blank;
extern bool open_tab_dl_list;
extern bool close_tab_back;
extern int nTab;
extern int TabCols;
extern bool check_target;

TabBuffer *newTab();
void calcTabPos();
TabBuffer *deleteTab(TabBuffer *tab);
TabBuffer *numTab(int n);
void moveTab(TabBuffer *t, TabBuffer *t2, int right);
void tabURL0(TabBuffer *tab, const char *prompt, int relative);
void pushBuffer(Buffer *buf);
void _newT();
void saveBufferInfo();
void followTab(TabBuffer *tab);
struct Str;
Str *currentURL(void);
void gotoLabel(const char *label);
int handleMailto(const char *url);
void goURL0(const char *prompt, int relative);
