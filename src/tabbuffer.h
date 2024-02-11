#pragma once
#include "url.h"
#include <optional>

struct Buffer;
struct FormList;
struct TabBuffer {
  TabBuffer *nextTab;
  TabBuffer *prevTab;

private:
  Buffer *_currentBuffer;

public:
  Buffer *currentBuffer() { return _currentBuffer; }
  void currentBuffer(Buffer *newbuf, bool first = false) {
    _currentBuffer = newbuf;
    if (first) {
      firstBuffer = newbuf;
    }
  }

  Buffer *firstBuffer;
  short x1;
  short x2;
  short y;
  void deleteBuffer(Buffer *delbuf);
  Buffer *namedBuffer(const char *name);
  void repBuffer(Buffer *oldbuf, Buffer *buf);
  void pushBuffer(Buffer *buf);
  bool select(char cmd, Buffer *buf);

  void cmd_loadURL(const char *url, std::optional<Url> current,
                   const char *referer, FormList *request);

  static void init(Buffer *newbuf);
  static TabBuffer *newTab();
  static void _newT();
};
#define NO_TABBUFFER ((TabBuffer *)1)

extern TabBuffer *CurrentTab;
#define Currentbuf (CurrentTab->currentBuffer())
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

void calcTabPos();
TabBuffer *deleteTab(TabBuffer *tab);
TabBuffer *numTab(int n);
void moveTab(TabBuffer *t, TabBuffer *t2, int right);
void tabURL0(TabBuffer *tab, const char *prompt, int relative);
void saveBufferInfo();
void followTab(TabBuffer *tab);
struct Str;
Str *currentURL(void);
void gotoLabel(const char *label);
int handleMailto(const char *url);
void goURL0(const char *prompt, int relative);
