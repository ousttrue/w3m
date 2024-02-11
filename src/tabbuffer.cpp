#include "tabbuffer.h"
#include "form.h"
#include "alloc.h"
#include "http_session.h"
#include "file_util.h"
#include "url_quote.h"
#include "url_stream.h"
#include "proto.h"
#include "rc.h"
#include "screen.h"
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
// #include "alloc.h"
#include "terms.h"
#include "buffer.h"
#include <sys/stat.h>

TabBuffer *CurrentTab;
TabBuffer *FirstTab;
TabBuffer *LastTab;
bool open_tab_blank = false;
bool open_tab_dl_list = false;
bool close_tab_back = false;
int nTab = 0;
int TabCols = 10;
bool check_target = true;

TabBuffer::TabBuffer() {}

TabBuffer::~TabBuffer() {}

void TabBuffer::init(const std::shared_ptr<Buffer> &newbuf) {
  FirstTab = LastTab = CurrentTab = new TabBuffer;
  if (!FirstTab) {
    fprintf(stderr, "%s\n", "Can't allocated memory");
    exit(1);
  }
  nTab = 1;
  Firstbuf = CurrentTab->_currentBuffer = newbuf;
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
  while (buf /*&& buf != NO_BUFFER*/) {
    auto next = buf->backBuffer;
    discardBuffer(buf);
    buf = next;
  }
  return FirstTab;
}

/*
 * deleteBuffer: delete buffer
 */
void TabBuffer::deleteBuffer(const std::shared_ptr<Buffer> &delbuf) {
  if (!delbuf) {
    return;
  }

  if (_currentBuffer == delbuf) {
    _currentBuffer = delbuf->backBuffer;
  }

  if (firstBuffer == delbuf && firstBuffer->backBuffer != nullptr) {
    auto buf = firstBuffer->backBuffer;
    discardBuffer(firstBuffer);
    firstBuffer = buf;
  } else if (auto buf = forwardBuffer(firstBuffer, delbuf)) {
    auto b = buf->backBuffer;
    buf->backBuffer = b->backBuffer;
    discardBuffer(b);
  }

  if (!Currentbuf) {
    _currentBuffer = Firstbuf;
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

  auto al = Currentbuf->layout.searchURLLabel(label);
  if (al == nullptr) {
    disp_message(Sprintf("%s is not found", label)->ptr, true);
    return;
  }
  auto buf = Buffer::create(Currentbuf->layout.width);
  *buf = *Currentbuf;
  for (int i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = nullptr;
  buf->info->currentURL.label = allocStr(label, -1);
  pushHashHist(URLHist, buf->info->currentURL.to_Str().c_str());
  buf->clone->count++;
  CurrentTab->pushBuffer(buf);
  Currentbuf->layout.gotoLine(al->start.line);
  if (label_topline)
    Currentbuf->layout.topLine =
        Currentbuf->layout.lineSkip(Currentbuf->layout.topLine,
                                    Currentbuf->layout.currentLine->linenumber -
                                        Currentbuf->layout.topLine->linenumber,
                                    false);
  Currentbuf->layout.pos = al->start.pos;
  Currentbuf->layout.arrangeCursor();
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

void TabBuffer::cmd_loadURL(const char *url, std::optional<Url> current,
                            const char *referer, FormList *request) {
  if (handleMailto((char *)url)) {
    return;
  }

  refresh(term_io());
  auto res = loadGeneralFile(url, current, {.referer = referer}, request);
  if (!res) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg, false);
    return;
  }

  auto buf = Buffer::create(INIT_BUFFER_WIDTH());
  buf->info = res;

  // if (buf != NO_BUFFER)
  { this->pushBuffer(buf); }

  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to specified URL */
void goURL0(const char *prompt, int relative) {
  auto url = searchKeyData();
  std::optional<Url> current;
  if (url == nullptr) {
    Hist *hist = copyHist(URLHist);

    current = Currentbuf->info->getBaseURL();
    if (current) {
      auto c_url = current->to_Str();
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url.c_str());
      else
        pushHist(hist, c_url);
    }
    auto a = retrieveCurrentAnchor(&Currentbuf->layout);
    if (a) {
      auto p_url = urlParse(a->url, current);
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

  const char *referer;
  if (relative) {
    const int *no_referer_ptr = nullptr;
    current = Currentbuf->info->getBaseURL();
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

  auto p_url = urlParse(url, current);
  pushHashHist(URLHist, p_url.to_Str().c_str());
  auto cur_buf = Currentbuf;
  CurrentTab->cmd_loadURL(url, current, referer, nullptr);
  if (Currentbuf != cur_buf) { /* success */
    pushHashHist(URLHist, Currentbuf->info->currentURL.to_Str().c_str());
  }
}

void tabURL0(TabBuffer *tab, const char *prompt, int relative) {
  if (tab == CurrentTab) {
    goURL0(prompt, relative);
    return;
  }
  TabBuffer::_newT();
  auto buf = Currentbuf;
  goURL0(prompt, relative);
  if (tab == nullptr) {
    if (buf != Currentbuf)
      CurrentTab->deleteBuffer(buf);
    else
      deleteTab(CurrentTab);
  } else if (buf != Currentbuf) {
    /* buf <- p <- ... <- Currentbuf = c */
    auto c = Currentbuf;
    std::shared_ptr<Buffer> p;
    if ((p = forwardBuffer(c, buf)))
      p->backBuffer = nullptr;
    Firstbuf = buf;
    deleteTab(CurrentTab);
    CurrentTab = tab;
    for (buf = p; buf; buf = p) {
      p = forwardBuffer(c, buf);
      CurrentTab->pushBuffer(buf);
    }
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void TabBuffer::pushBuffer(const std::shared_ptr<Buffer> &buf) {
  if (Firstbuf == Currentbuf) {
    buf->backBuffer = Firstbuf;
    Firstbuf = buf;
  } else if (auto b = forwardBuffer(Firstbuf, Currentbuf)) {
    buf->backBuffer = Currentbuf;
    b->backBuffer = buf;
  }
  _currentBuffer = buf;
  saveBufferInfo();
}

void TabBuffer::_newT() {
  auto tag = new TabBuffer();
  auto buf = Buffer::create(Currentbuf->layout.width);
  *buf = *Currentbuf;
  buf->backBuffer = nullptr;
  for (int i = 0; i < MAX_LB; i++) {
    buf->linkBuffer[i] = nullptr;
  }
  buf->clone->count++;
  tag->firstBuffer = tag->_currentBuffer = buf;

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
  auto a = retrieveCurrentAnchor(&Currentbuf->layout);
  if (a == nullptr)
    return;

  if (tab == CurrentTab) {
    check_target = false;
    followA();
    check_target = true;
    return;
  }
  TabBuffer::_newT();
  auto buf = Currentbuf;
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
    auto c = Currentbuf;
    std::shared_ptr<Buffer> p;
    if ((p = forwardBuffer(c, buf)))
      p->backBuffer = nullptr;
    Firstbuf = buf;
    deleteTab(CurrentTab);
    CurrentTab = tab;
    for (buf = p; buf; buf = p) {
      p = forwardBuffer(c, buf);
      CurrentTab->pushBuffer(buf);
    }
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* show current URL */
Str *currentURL(void) { return Strnew(Currentbuf->info->currentURL.to_Str()); }

void SAVE_BUFPOSITION(LineLayout *sbufp) {
  sbufp->COPY_BUFPOSITION_FROM(Currentbuf->layout);
}
void RESTORE_BUFPOSITION(const LineLayout &sbufp) {
  Currentbuf->layout.COPY_BUFPOSITION_FROM(sbufp);
}

/*
 * namedBuffer: Select buffer which have specified name
 */
std::shared_ptr<Buffer> namedBuffer(const std::shared_ptr<Buffer> &first,
                                    char *name) {
  if (first->layout.title == name) {
    return first;
  }
  for (auto buf = first; buf->backBuffer != nullptr; buf = buf->backBuffer) {
    if (buf->backBuffer->layout.title == name) {
      return buf->backBuffer;
    }
  }
  return nullptr;
}

void TabBuffer::repBuffer(const std::shared_ptr<Buffer> &oldbuf,
                          const std::shared_ptr<Buffer> &buf) {
  Firstbuf = replaceBuffer(oldbuf, buf);
  _currentBuffer = buf;
}

bool TabBuffer::select(char cmd, const std::shared_ptr<Buffer> &buf) {
  switch (cmd) {
  case 'B':
    return true;

  case '\n':
  case ' ':
    _currentBuffer = buf;
    return true;

  case 'D':
    CurrentTab->deleteBuffer(buf);
    if (Firstbuf == nullptr) {
      /* No more buffer */
      Firstbuf = nullBuffer();
      _currentBuffer = Firstbuf;
    }
    break;

  case 'q':
    qquitfm();
    break;

  case 'Q':
    quitfm();
    break;
  }

  return false;
}

/*
 * replaceBuffer: replace buffer
 */
std::shared_ptr<Buffer>
TabBuffer::replaceBuffer(const std::shared_ptr<Buffer> &delbuf,
                         const std::shared_ptr<Buffer> &newbuf) {

  auto first = Firstbuf;
  if (delbuf == nullptr) {
    newbuf->backBuffer = first;
    return newbuf;
  }

  if (first == delbuf) {
    newbuf->backBuffer = delbuf->backBuffer;
    discardBuffer(delbuf);
    return newbuf;
  }

  std::shared_ptr<Buffer> buf;
  if (delbuf && (buf = forwardBuffer(first, delbuf))) {
    buf->backBuffer = newbuf;
    newbuf->backBuffer = delbuf->backBuffer;
    discardBuffer(delbuf);
    return first;
  }
  newbuf->backBuffer = first;
  return newbuf;
}

std::shared_ptr<Buffer> TabBuffer::loadLink(const char *url, const char *target,
                                            const char *referer,
                                            FormList *request) {
  message(Sprintf("loading %s", url)->ptr, 0, 0);
  refresh(term_io());

  const int *no_referer_ptr = nullptr;
  auto base = this->currentBuffer()->info->getBaseURL();
  if ((no_referer_ptr && *no_referer_ptr) || !base ||
      base->schema == SCM_LOCAL || base->schema == SCM_LOCAL_CGI ||
      base->schema == SCM_DATA)
    referer = NO_REFERER;
  if (referer == nullptr)
    referer =
        Strnew(this->currentBuffer()->info->currentURL.to_RefererStr())->ptr;

  auto res = loadGeneralFile(url, this->currentBuffer()->info->getBaseURL(),
                             {.referer = referer}, request);
  if (!res) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg, false);
    return nullptr;
  }

  auto buf = Buffer::create(INIT_BUFFER_WIDTH());
  buf->info = res;

  auto pu = urlParse(url, base);
  pushHashHist(URLHist, pu.to_Str().c_str());

  // if (buf == NO_BUFFER) {
  //   return nullptr;
  // }
  if (!on_target) { /* open link as an indivisual page */
    this->pushBuffer(buf);
    return buf;
  }

  // if (do_download) /* download (thus no need to render frames) */
  //   return loadNormalBuf(buf, false);

  if (target == nullptr || /* no target specified (that means this page is not a
                           frame page) */
      !strcmp(target, "_top") /* this link is specified to be opened as an
                                 indivisual * page */
  ) {
    this->pushBuffer(buf);
    return buf;
  }
  auto nfbuf = this->currentBuffer()->linkBuffer[LB_N_FRAME];
  if (nfbuf == nullptr) {
    /* original page (that contains <frameset> tag) doesn't exist */
    this->pushBuffer(buf);
    return buf;
  }

  {
    Anchor *al = nullptr;
    auto label = pu.label;

    if (!al) {
      label = Strnew_m_charp("_", target, nullptr)->ptr;
      al = this->currentBuffer()->layout.searchURLLabel(label.c_str());
    }
    if (al) {
      this->currentBuffer()->layout.gotoLine(al->start.line);
      if (label_topline)
        this->currentBuffer()->layout.topLine =
            this->currentBuffer()->layout.lineSkip(
                this->currentBuffer()->layout.topLine,
                this->currentBuffer()->layout.currentLine->linenumber -
                    this->currentBuffer()->layout.topLine->linenumber,
                false);
      this->currentBuffer()->layout.pos = al->start.pos;
      this->currentBuffer()->layout.arrangeCursor();
    }
  }
  displayBuffer(B_NORMAL);
  return buf;
}

static FormItemList *save_submit_formlist(FormItemList *src) {
  FormList *list;
  FormList *srclist;
  FormItemList *srcitem;
  FormItemList *item;
  FormItemList *ret = nullptr;

  if (src == nullptr)
    return nullptr;
  srclist = src->parent;
  list = (FormList *)New(FormList);
  list->method = srclist->method;
  list->action = srclist->action->Strdup();
  list->enctype = srclist->enctype;
  list->nitems = srclist->nitems;
  list->body = srclist->body;
  list->boundary = srclist->boundary;
  list->length = srclist->length;

  for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
    item = (FormItemList *)New(FormItemList);
    item->type = srcitem->type;
    item->name = srcitem->name->Strdup();
    item->value = srcitem->value->Strdup();
    item->checked = srcitem->checked;
    item->accept = srcitem->accept;
    item->size = srcitem->size;
    item->rows = srcitem->rows;
    item->maxlength = srcitem->maxlength;
    item->readonly = srcitem->readonly;
    item->parent = list;
    item->next = nullptr;

    if (list->lastitem == nullptr) {
      list->item = list->lastitem = item;
    } else {
      list->lastitem->next = item;
      list->lastitem = item;
    }

    if (srcitem == src)
      ret = item;
  }

  return ret;
}

void TabBuffer::do_submit(FormItemList *fi, Anchor *a) {
  auto tmp = Strnew();
  auto multipart = (fi->parent->method == FORM_METHOD_POST &&
                    fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
  query_from_followform(&tmp, fi, multipart);

  auto tmp2 = fi->parent->action->Strdup();
  if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
    /* It means "current URL" */
    tmp2 = Strnew(currentBuffer()->info->currentURL.to_Str());
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
  }

  if (fi->parent->method == FORM_METHOD_GET) {
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    Strcat_charp(tmp2, "?");
    Strcat(tmp2, tmp);
    this->loadLink(tmp2->ptr, a->target, nullptr, nullptr);
  } else if (fi->parent->method == FORM_METHOD_POST) {
    if (multipart) {
      struct stat st;
      stat(fi->parent->body, &st);
      fi->parent->length = st.st_size;
    } else {
      fi->parent->body = tmp->ptr;
      fi->parent->length = tmp->length;
    }
    auto buf = this->loadLink(tmp2->ptr, a->target, nullptr, fi->parent);
    if (multipart) {
      unlink(fi->parent->body);
    }
    if (buf &&
        !(buf->info->redirectins.size() > 1)) { /* buf must be Currentbuf */
      /* BP_REDIRECTED means that the buffer is obtained through
       * Location: header. In this case, buf->form_submit must not be set
       * because the page is not loaded by POST method but GET method.
       */
      buf->layout.form_submit = save_submit_formlist(fi);
    }
  } else if ((fi->parent->method == FORM_METHOD_INTERNAL &&
              (!Strcmp_charp(fi->parent->action, "map") ||
               !Strcmp_charp(fi->parent->action, "none")))) { /* internal */
    do_internal(tmp2->ptr, tmp->ptr);
  } else {
    disp_err_message("Can't send form because of illegal method.", false);
  }
  displayBuffer(B_FORCE_REDRAW);
}
