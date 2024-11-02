#include "tabbuffer.h"
#include "alloc.h"
#include "buffer.h"
#include "buffer/message.h"
#include "document.h"
#include "input/loader.h"
#include "term/termsize.h"
#include "text/text.h"
#include <string.h>

bool clear_buffer = true;
bool close_tab_back = false;
bool open_tab_blank = false;

int nTab = 0;
int TabCols = 10;

struct TabBuffer *newTab(void) {
  auto n = New(struct TabBuffer);
  n->nextTab = nullptr;
  n->currentBuffer = nullptr;
  n->firstBuffer = nullptr;
  return n;
}

void tabInitialize(struct Document *doc) {
  auto buf = newBuffer();
  buf->document = doc;
  if (!CurrentTab) {
    FirstTab = LastTab = CurrentTab = newTab();
    nTab = 1;
    Firstbuf = Currentbuf = buf;
  } else {
    Currentbuf->nextBuffer = buf;
    Currentbuf = buf;
  }
}

void _newT(struct Buffer *buf) {
  if (!buf) {
    return;
  }
  auto tag = newTab();
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

void pushBuffer(struct TabBuffer *tab, struct Buffer *buf) {
  if (clear_buffer)
    tmpClearBuffer(tab->currentBuffer->document);

  struct Buffer *b;
  if (tab->firstBuffer == tab->currentBuffer) {
    buf->nextBuffer = tab->firstBuffer;
    tab->firstBuffer = tab->currentBuffer = buf;
  } else if ((b = prevBuffer(tab->firstBuffer, tab->currentBuffer)) != NULL) {
    b->nextBuffer = buf;
    buf->nextBuffer = tab->currentBuffer;
    tab->currentBuffer = buf;
  }
  saveBufferInfo();
}

void pushCheckTarget(struct TabBuffer *tab, const char *anchor_target,
                     struct Buffer *buf, bool check_target) {
  if (check_target && open_tab_blank && anchor_target &&
      (!strcasecmp(anchor_target, "_new") ||
       !strcasecmp(anchor_target, "_blank"))) {
    _newT(buf);
  } else {
    pushBuffer(tab, buf);
  }
}

bool handleMailto(const char *url) {
  return false;
  // if (strncasecmp(url, "mailto:", 7))
  //   return 0;
  // if (!non_null(Mailer)) {
  //   message_push("no mailer is specified");
  //   return 1;
  // }
  //
  // /* invoke external mailer */
  // Str to;
  // if (MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL) {
  //   to = Strnew_charp(html_unquote(url));
  // } else {
  //   to = Strnew_charp(url + 7);
  //   char *pos;
  //   if ((pos = strchr(to->ptr, '?')) != NULL)
  //     Strtruncate(to, pos - to->ptr);
  // }
  // term_fmTerm();
  // system(myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)),
  // false)->ptr); term_fmInit(); displayBuffer(Currentbuf, B_FORCE_REDRAW);
  // pushHashHist(URLHist, url);
  // return 1;
}

void moveTab(struct TabBuffer *t, struct TabBuffer *t2, int right) {
  if (t2 == NO_TABBUFFER)
    t2 = FirstTab;
  if (!t || !t2 || t == t2 || t == NO_TABBUFFER)
    return;
  if (t->prevTab) {
    if (t->nextTab)
      t->nextTab->prevTab = t->prevTab;
    else
      LastTab = t->prevTab;
    t->prevTab->nextTab = t->nextTab;
  } else {
    t->nextTab->prevTab = NULL;
    FirstTab = t->nextTab;
  }
  if (right) {
    t->nextTab = t2->nextTab;
    t->prevTab = t2;
    if (t2->nextTab)
      t2->nextTab->prevTab = t;
    else
      LastTab = t;
    t2->nextTab = t;
  } else {
    t->prevTab = t2->prevTab;
    t->nextTab = t2;
    if (t2->prevTab)
      t2->prevTab->nextTab = t;
    else
      FirstTab = t;
    t2->prevTab = t;
  }
  // displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static int checkBackBuffer(struct Buffer *buf) {
  if (buf->nextBuffer)
    return true;

  return false;
}

void _backBf(struct TabBuffer *tab) {
  if (!checkBackBuffer(tab->currentBuffer)) {
    // last buffer. close tab
    if (close_tab_back && nTab >= 1) {
      deleteTab(CurrentTab);
    } else {
      message_push("Can't go back...");
    }
    return;
  }

  delBuffer(tab, Currentbuf);
}

void delBuffer(struct TabBuffer *tab, struct Buffer *buf) {
  if (!buf) {
    return;
  }
  if (tab->currentBuffer == buf) {
    tab->currentBuffer = buf->nextBuffer;
  }
  tab->firstBuffer = deleteBuffer(Firstbuf, buf);
  if (!tab->currentBuffer) {
    tab->currentBuffer = tab->firstBuffer;
  }
}
