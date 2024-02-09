#include "tabbuffer.h"
#include "url_quote.h"
#include "url_stream.h"
#include "proto.h"
#include "rc.h"
#include "screen.h"
#include "loadproc.h"
#include "util.h"
#include "etc.h"
#include "message.h"
#include "buffer.h"
#include "http_response.h"
#include "myctype.h"
#include "anchor.h"
#include "w3m.h"
#include "history.h"
#include "app.h"
#include "display.h"
#include "alloc.h"
#include "terms.h"
#include "buffer.h"

TabBuffer *CurrentTab;
TabBuffer *FirstTab;
TabBuffer *LastTab;
bool open_tab_blank = false;
bool open_tab_dl_list = false;
bool close_tab_back = false;
int nTab = 0;
int TabCols = 10;
bool check_target = true;

TabBuffer *newTab(void) {

  auto n = (TabBuffer *)New(TabBuffer);
  if (n == nullptr)
    return nullptr;
  n->nextTab = nullptr;
  n->currentBuffer = nullptr;
  n->firstBuffer = nullptr;
  return n;
}

TabBuffer *numTab(int n) {
  TabBuffer *tab;
  int i;

  if (n == 0)
    return CurrentTab;
  if (n == 1)
    return FirstTab;
  if (nTab <= 1)
    return nullptr;
  for (tab = FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
    ;
  return tab;
}

void calcTabPos(void) {
  TabBuffer *tab;
  int lcol = 0, rcol = 0, col;
  int n1, n2, na, nx, ny, ix, iy;

  if (nTab <= 0)
    return;
  n1 = (COLS - rcol - lcol) / TabCols;
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
  na = n1 + n2 * (ny - 1);
  n1 -= (na - nTab) / ny;
  if (n1 < 0)
    n1 = 0;
  na = n1 + n2 * (ny - 1);
  tab = FirstTab;
  for (iy = 0; iy < ny && tab; iy++) {
    if (iy == 0) {
      nx = n1;
      col = COLS - rcol - lcol;
    } else {
      nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
      col = COLS;
    }
    for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
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

TabBuffer *deleteTab(TabBuffer *tab) {
  Buffer *buf, *next;

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
  buf = tab->firstBuffer;
  while (buf && buf != NO_BUFFER) {
    next = buf->nextBuffer;
    discardBuffer(buf);
    buf = next;
  }
  return FirstTab;
}

/*
 * deleteBuffer: delete buffer
 */
void TabBuffer::deleteBuffer(Buffer *delbuf) {
  if (!delbuf) {
    return;
  }

  if (Currentbuf == delbuf) {
    Currentbuf = delbuf->nextBuffer;
  }

  if (firstBuffer == delbuf && firstBuffer->nextBuffer != nullptr) {
    auto buf = firstBuffer->nextBuffer;
    discardBuffer(firstBuffer);
    firstBuffer = buf;
  } else if (auto buf = prevBuffer(firstBuffer, delbuf)) {
    auto b = buf->nextBuffer;
    buf->nextBuffer = b->nextBuffer;
    discardBuffer(b);
  }

  if (!Currentbuf) {
    Currentbuf = Firstbuf;
  }
}

void moveTab(TabBuffer *t, TabBuffer *t2, int right) {
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
    t->nextTab->prevTab = nullptr;
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
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void gotoLabel(const char *label) {
  Buffer *buf;
  Anchor *al;
  int i;

  al = searchURLLabel(Currentbuf, label);
  if (al == nullptr) {
    disp_message(Sprintf("%s is not found", label)->ptr, true);
    return;
  }
  buf = new Buffer(Currentbuf->width);
  *buf = *Currentbuf;
  for (i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = nullptr;
  buf->info->currentURL.label = allocStr(label, -1);
  pushHashHist(URLHist, buf->info->currentURL.to_Str().c_str());
  buf->clone->count++;
  pushBuffer(buf);
  gotoLine(Currentbuf, al->start.line);
  if (label_topline)
    Currentbuf->layout.topLine = lineSkip(Currentbuf, Currentbuf->layout.topLine,
                                   Currentbuf->layout.currentLine->linenumber -
                                       Currentbuf->layout.topLine->linenumber,
                                   false);
  Currentbuf->pos = al->start.pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  return;
}

int handleMailto(const char *url) {
  Str *to;
  char *pos;

  if (strncasecmp(url, "mailto:", 7))
    return 0;
  if (!non_null(Mailer)) {
    disp_err_message("no mailer is specified", true);
    return 1;
  }

  /* invoke external mailer */
  if (MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL) {
    to = Strnew_charp(html_unquote((char *)url));
  } else {
    to = Strnew_charp(url + 7);
    if ((pos = strchr(to->ptr, '?')) != nullptr)
      Strtruncate(to, pos - to->ptr);
  }
  exec_cmd(
      myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)), false)->ptr);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  pushHashHist(URLHist, (char *)url);
  return 1;
}

void cmd_loadURL(const char *url, std::optional<Url> current,
                 const char *referer, FormList *request) {
  if (handleMailto((char *)url))
    return;

  refresh(term_io());
  auto buf = loadGeneralFile(url, current, {.referer = referer}, request);
  if (buf == nullptr) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to specified URL */
void goURL0(const char *prompt, int relative) {
  const char *url, *referer;
  Url p_url;
  Buffer *cur_buf = Currentbuf;
  const int *no_referer_ptr;

  url = searchKeyData();
  std::optional<Url> current;
  if (url == nullptr) {
    Hist *hist = copyHist(URLHist);
    Anchor *a;

    current = baseURL(Currentbuf);
    if (current) {
      auto c_url = current->to_Str();
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url.c_str());
      else
        pushHist(hist, c_url);
    }
    a = retrieveCurrentAnchor(Currentbuf);
    if (a) {
      p_url = urlParse(a->url, current);
      auto a_url = p_url.to_Str();
      if (DefaultURLString == DEFAULT_URL_LINK)
        url = url_decode0(a_url.c_str());
      else
        pushHist(hist, a_url);
    }
    // url = inputLineHist(prompt, url, IN_URL, hist);
    if (url != nullptr)
      SKIP_BLANKS(url);
  }
  if (relative) {
    no_referer_ptr = nullptr;
    current = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || !current ||
        current->schema == SCM_LOCAL || current->schema == SCM_LOCAL_CGI ||
        current->schema == SCM_DATA)
      referer = NO_REFERER;
    else
      referer = Strnew(Currentbuf->info->currentURL.to_RefererStr())->ptr;
    url = Strnew(url_quote(url))->ptr;
  } else {
    current = {};
    referer = nullptr;
    url = Strnew(url_quote(url))->ptr;
  }
  if (url == nullptr || *url == '\0') {
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  if (*url == '#') {
    gotoLabel(url + 1);
    return;
  }
  p_url = urlParse(url, current);
  pushHashHist(URLHist, p_url.to_Str().c_str());
  cmd_loadURL(url, current, referer, nullptr);
  if (Currentbuf != cur_buf) /* success */
    pushHashHist(URLHist, Currentbuf->info->currentURL.to_Str().c_str());
}

void tabURL0(TabBuffer *tab, const char *prompt, int relative) {
  Buffer *buf;

  if (tab == CurrentTab) {
    goURL0(prompt, relative);
    return;
  }
  _newT();
  buf = Currentbuf;
  goURL0(prompt, relative);
  if (tab == nullptr) {
    if (buf != Currentbuf)
      CurrentTab->deleteBuffer(buf);
    else
      deleteTab(CurrentTab);
  } else if (buf != Currentbuf) {
    /* buf <- p <- ... <- Currentbuf = c */
    Buffer *c, *p;

    c = Currentbuf;
    if ((p = prevBuffer(c, buf)))
      p->nextBuffer = nullptr;
    Firstbuf = buf;
    deleteTab(CurrentTab);
    CurrentTab = tab;
    for (buf = p; buf; buf = p) {
      p = prevBuffer(c, buf);
      pushBuffer(buf);
    }
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void pushBuffer(Buffer *buf) {
  Buffer *b;

  if (Firstbuf == Currentbuf) {
    buf->nextBuffer = Firstbuf;
    Firstbuf = Currentbuf = buf;
  } else if ((b = prevBuffer(Firstbuf, Currentbuf)) != nullptr) {
    b->nextBuffer = buf;
    buf->nextBuffer = Currentbuf;
    Currentbuf = buf;
  }
  saveBufferInfo();
}

void _newT() {
  TabBuffer *tag;
  Buffer *buf;
  int i;

  tag = newTab();
  if (!tag)
    return;

  buf = new Buffer(Currentbuf->width);
  *buf = *Currentbuf;
  buf->nextBuffer = nullptr;
  for (i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = nullptr;
  buf->clone->count++;
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

void saveBufferInfo() {
  FILE *fp;
  if ((fp = fopen(rcFile("bufinfo"), "w")) == nullptr) {
    return;
  }
  fprintf(fp, "%s\n", currentURL()->ptr);
  fclose(fp);
}

void followTab(TabBuffer *tab) {
  Buffer *buf;
  Anchor *a;

  a = retrieveCurrentAnchor(Currentbuf);
  if (a == nullptr)
    return;

  if (tab == CurrentTab) {
    check_target = false;
    followA();
    check_target = true;
    return;
  }
  _newT();
  buf = Currentbuf;
  check_target = false;
  followA();
  check_target = true;
  if (tab == nullptr) {
    if (buf != Currentbuf)
      CurrentTab->deleteBuffer(buf);
    else
      deleteTab(CurrentTab);
  } else if (buf != Currentbuf) {
    /* buf <- p <- ... <- Currentbuf = c */
    Buffer *c, *p;

    c = Currentbuf;
    if ((p = prevBuffer(c, buf)))
      p->nextBuffer = nullptr;
    Firstbuf = buf;
    deleteTab(CurrentTab);
    CurrentTab = tab;
    for (buf = p; buf; buf = p) {
      p = prevBuffer(c, buf);
      pushBuffer(buf);
    }
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* show current URL */
Str *currentURL(void) { return Strnew(Currentbuf->info->currentURL.to_Str()); }

void SAVE_BUFPOSITION(Buffer *sbufp) { COPY_BUFPOSITION(sbufp, Currentbuf); }
void RESTORE_BUFPOSITION(Buffer *sbufp) { COPY_BUFPOSITION(Currentbuf, sbufp); }
