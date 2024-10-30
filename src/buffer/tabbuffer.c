#include "tabbuffer.h"
#include "alloc.h"
#include "buffer.h"
#include "buffer/message.h"
#include "document.h"
#include "input/loader.h"
#include "term/termsize.h"

bool clear_buffer = true;

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

void cmd_loadURL(struct TabBuffer *tab, const char *url, struct Url *current,
                 const char *referer, struct FormList *form) {
  if (handleMailto(url))
    return;

  // term_refresh();
  auto buf = loadGeneralFile(INIT_BUFFER_WIDTH, url, current, referer, 0, form);
  if (buf == NULL) {
    const char *emsg = Sprintf("Can't load %s", url)->ptr;
    message_push(emsg);
    return;
  }

  if (buf != NO_BUFFER) {
    pushBuffer(tab, buf);
  }
}
