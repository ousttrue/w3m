#include "tabbuffer.h"
#include "buffer.h"
#include "line_layout.h"
#include "utf8.h"
#include "Str.h"
#include "linein.h"
#include "html/form.h"
#include "alloc.h"
#include "http_session.h"
#include "http_response.h"
#include "url_quote.h"
#include "proto.h"
#include "screen.h"
#include "message.h"
#include "myctype.h"
#include "w3m.h"
#include "history.h"
#include "app.h"
#include <sys/stat.h>

bool open_tab_blank = false;
bool open_tab_dl_list = false;
bool close_tab_back = false;
int TabCols = 10;

TabBuffer::TabBuffer() {}

TabBuffer::~TabBuffer() {}

int TabBuffer::draw(TabBuffer *current) {
  move(this->y, this->x1);
  if (this == current) {
    bold();
  } else {
    boldend();
  }
  addch('[');
  auto l = this->x2 - this->x1 - 1 -
           get_strwidth(this->currentBuffer()->layout.title.c_str());
  if (l < 0) {
    l = 0;
  }
  if (l / 2 > 0) {
    addnstr_sup(" ", l / 2);
  }

  if (this == current) {
    underline();
    // standout();
  }
  addnstr(this->currentBuffer()->layout.title.c_str(), this->x2 - this->x1 - l);
  if (this == current) {
    underlineend();
    // standend();
  }

  if ((l + 1) / 2 > 0) {
    addnstr_sup(" ", (l + 1) / 2);
  }
  move(this->y, this->x2);
  addch(']');
  if (this == current) {
    boldend();
  }
  return this->y;
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
    firstBuffer = firstBuffer->backBuffer;
  } else if (auto buf = forwardBuffer(firstBuffer, delbuf)) {
  }

  if (!this->currentBuffer()) {
    _currentBuffer = this->firstBuffer;
  }
}

std::shared_ptr<Buffer> TabBuffer::gotoLabel(const char *label) {

  auto al = this->currentBuffer()->layout.name()->searchAnchor(label);
  if (al == nullptr) {
    disp_message(Sprintf("%s is not found", label)->ptr);
    return {};
  }

  auto buf = Buffer::create();
  *buf = *this->currentBuffer();
  for (int i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = nullptr;
  buf->res->currentURL.label = allocStr(label, -1);
  pushHashHist(URLHist, buf->res->currentURL.to_Str().c_str());
  this->pushBuffer(buf);
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
  return buf;
}

void TabBuffer::cmd_loadURL(const char *url, std::optional<Url> current,
                            const HttpOption &option, FormList *request) {

  // refresh(term_io());
  auto res = loadGeneralFile(url, current, option, request);
  if (!res) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg);
    return;
  }

  auto buf = Buffer::create(res);

  // if (buf != NO_BUFFER)
  { this->pushBuffer(buf); }
}

/* go to specified URL */
void TabBuffer::goURL0(const char *prompt, bool relative) {
  auto url = App::instance().searchKeyData();
  std::optional<Url> current;
  if (url == nullptr) {
    Hist *hist = copyHist(URLHist);

    current = this->currentBuffer()->res->getBaseURL();
    if (current) {
      auto c_url = current->to_Str();
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url.c_str());
      else
        pushHist(hist, c_url);
    }
    auto a = this->currentBuffer()->layout.retrieveCurrentAnchor();
    if (a) {
      auto p_url = urlParse(a->url, current);
      auto a_url = p_url.to_Str();
      if (DefaultURLString == DEFAULT_URL_LINK)
        url = url_decode0(a_url.c_str());
      else
        pushHist(hist, a_url);
    }
    // url = inputLineHist(prompt, url, IN_URL, hist);
    if (url != nullptr) {
      SKIP_BLANKS(url);
    }
  }

  HttpOption option = {};
  if (relative) {
    const int *no_referer_ptr = nullptr;
    current = this->currentBuffer()->res->getBaseURL();
    if ((no_referer_ptr && *no_referer_ptr) || !current ||
        current->schema == SCM_LOCAL || current->schema == SCM_LOCAL_CGI ||
        current->schema == SCM_DATA)
      option.no_referer = true;
    else
      option.referer = this->currentBuffer()->res->currentURL.to_RefererStr();
    url = Strnew(url_quote(url))->ptr;
  } else {
    current = {};
    url = Strnew(url_quote(url))->ptr;
  }

  if (url == nullptr || *url == '\0') {
    return;
  }
  if (*url == '#') {
    this->gotoLabel(url + 1);
    return;
  }

  auto p_url = urlParse(url, current);
  pushHashHist(URLHist, p_url.to_Str().c_str());
  auto cur_buf = this->currentBuffer();
  this->cmd_loadURL(url, current, option, nullptr);
  if (this->currentBuffer() != cur_buf) { /* success */
    pushHashHist(URLHist,
                 this->currentBuffer()->res->currentURL.to_Str().c_str());
  }
}

void TabBuffer::pushBuffer(const std::shared_ptr<Buffer> &buf) {
  if (this->firstBuffer == this->currentBuffer()) {
    buf->backBuffer = this->firstBuffer;
    this->firstBuffer = buf;
  } else if (auto b = forwardBuffer(this->firstBuffer, this->currentBuffer())) {
    buf->backBuffer = this->currentBuffer();
    b->backBuffer = buf;
  }
  _currentBuffer = buf;
  this->currentBuffer()->saveBufferInfo();
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
  this->firstBuffer = replaceBuffer(oldbuf, buf);
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
    this->deleteBuffer(buf);
    if (this->firstBuffer == nullptr) {
      /* No more buffer */
      this->firstBuffer = nullBuffer();
      _currentBuffer = this->firstBuffer;
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

  auto first = this->firstBuffer;
  if (delbuf == nullptr) {
    newbuf->backBuffer = first;
    return newbuf;
  }

  if (first == delbuf) {
    newbuf->backBuffer = delbuf->backBuffer;
    return newbuf;
  }

  std::shared_ptr<Buffer> buf;
  if (delbuf && (buf = forwardBuffer(first, delbuf))) {
    buf->backBuffer = newbuf;
    newbuf->backBuffer = delbuf->backBuffer;
    return first;
  }
  newbuf->backBuffer = first;
  return newbuf;
}

std::shared_ptr<Buffer> TabBuffer::loadLink(const char *url, const char *target,
                                            HttpOption option,
                                            FormList *request) {
  message(Sprintf("loading %s", url)->ptr, 0, 0);
  // refresh(term_io());

  const int *no_referer_ptr = nullptr;
  auto base = this->currentBuffer()->res->getBaseURL();

  if ((no_referer_ptr && *no_referer_ptr) || !base ||
      base->schema == SCM_LOCAL || base->schema == SCM_LOCAL_CGI ||
      base->schema == SCM_DATA) {
    option.no_referer = true;
  }
  if (option.referer.empty()) {
    option.referer = this->currentBuffer()->res->currentURL.to_RefererStr();
  }

  auto res = loadGeneralFile(url, this->currentBuffer()->res->getBaseURL(),
                             option, request);
  if (!res) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg);
    return nullptr;
  }

  auto buf = Buffer::create(res);

  auto pu = urlParse(url, base);
  pushHashHist(URLHist, pu.to_Str().c_str());

  // if (buf == NO_BUFFER) {
  //   return nullptr;
  // }
  // if (!on_target) { /* open link as an indivisual page */
  //   this->pushBuffer(buf);
  //   return buf;
  // }

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
      al = this->currentBuffer()->layout.name()->searchAnchor(label);
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
  // App::instance().invalidate();
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

std::shared_ptr<Buffer> TabBuffer::do_submit(FormItemList *fi, Anchor *a) {
  auto tmp2 = fi->parent->action->Strdup();
  if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
    /* It means "current URL" */
    tmp2 = Strnew(currentBuffer()->res->currentURL.to_Str());
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
  }

  if (fi->parent->method == FORM_METHOD_GET) {
    auto tmp = fi->query_from_followform();
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    Strcat_charp(tmp2, "?");
    Strcat(tmp2, tmp);
    return this->loadLink(tmp2->ptr, a->target, {}, nullptr);
  } else if (fi->parent->method == FORM_METHOD_POST) {
    if (fi->parent->enctype == FORM_ENCTYPE_MULTIPART) {
      fi->query_from_followform_multipart();
      struct stat st;
      stat(fi->parent->body, &st);
      fi->parent->length = st.st_size;
    } else {
      auto tmp = fi->query_from_followform();
      fi->parent->body = tmp->ptr;
      fi->parent->length = tmp->length;
    }
    auto buf = this->loadLink(tmp2->ptr, a->target, {}, fi->parent);
    if (fi->parent->enctype == FORM_ENCTYPE_MULTIPART) {
      unlink(fi->parent->body);
    }
    if (buf &&
        !(buf->res->redirectins.size() > 1)) { /* buf must be Currentbuf */
      /* BP_REDIRECTED means that the buffer is obtained through
       * Location: header. In this case, buf->form_submit must not be set
       * because the page is not loaded by POST method but GET method.
       */
      buf->layout.form_submit = save_submit_formlist(fi);
    }
    return buf;
  } else if ((fi->parent->method == FORM_METHOD_INTERNAL &&
              (!Strcmp_charp(fi->parent->action, "map") ||
               !Strcmp_charp(fi->parent->action, "none")))) { /* internal */
    auto tmp = fi->query_from_followform();
    do_internal(tmp2->ptr, tmp->ptr);
    return {};
  } else {
    disp_err_message("Can't send form because of illegal method.");
    return {};
  }
}

std::shared_ptr<Buffer> TabBuffer::_followForm(int submit) {
  if (!currentBuffer()->layout.firstLine) {
    return {};
  }

  auto a = currentBuffer()->layout.retrieveCurrentForm();
  if (!a) {
    return {};
  }
  auto fi = (FormItemList *)a->url;

  switch (fi->type) {
  case FORM_INPUT_TEXT:
    if (submit) {
      return this->do_submit(fi, a);
    }
    if (fi->readonly)
      disp_message_nsec("Read only field!", 1, true);
    inputStrHist("TEXT:", fi->value ? fi->value->ptr : nullptr, TextHist,
                 [fi, a](const char *p) {
                   if (p == nullptr || fi->readonly) {
                     return;
                     // break;
                   }
                   fi->value = Strnew_charp(p);
                   formUpdateBuffer(a, &CurrentTab->currentBuffer()->layout,
                                    fi);
                   if (fi->accept || fi->parent->nitems == 1) {
                     CurrentTab->do_submit(fi, a);
                     return;
                   }
                   App::instance().invalidate();
                 });
    break;

  case FORM_INPUT_FILE:
    if (submit) {
      return this->do_submit(fi, a);
    }

    if (fi->readonly)
      disp_message_nsec("Read only field!", 1, true);

    // p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : nullptr,
    // nullptr); if (p == nullptr || fi->readonly)
    //   break;
    //
    // fi->value = Strnew_charp(p);
    // formUpdateBuffer(a, Currentbuf, fi);
    // if (fi->accept || fi->parent->nitems == 1)
    //   goto do_submit;
    break;
  case FORM_INPUT_PASSWORD:
    if (submit) {
      return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", 1, true);
      break;
    }
    // p = inputLine("Password:", fi->value ? fi->value->ptr : nullptr,
    // IN_PASSWORD);
    // if (p == nullptr)
    //   break;
    // fi->value = Strnew_charp(p);
    formUpdateBuffer(a, &currentBuffer()->layout, fi);
    if (fi->accept) {
      return this->do_submit(fi, a);
    }
    break;
  case FORM_TEXTAREA:
    if (submit) {
      return this->do_submit(fi, a);
    }
    if (fi->readonly)
      disp_message_nsec("Read only field!", 1, true);
    input_textarea(fi);
    formUpdateBuffer(a, &currentBuffer()->layout, fi);
    break;
  case FORM_INPUT_RADIO:
    if (submit) {
      return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", 1, true);
      break;
    }
    formRecheckRadio(a, currentBuffer(), fi);
    break;
  case FORM_INPUT_CHECKBOX:
    if (submit) {
      return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", 1, true);
      break;
    }
    fi->checked = !fi->checked;
    formUpdateBuffer(a, &currentBuffer()->layout, fi);
    break;
  case FORM_INPUT_IMAGE:
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
    this->do_submit(fi, a);
    break;

  case FORM_INPUT_RESET:
    for (size_t i = 0; i < currentBuffer()->layout.formitem()->size(); i++) {
      auto a2 = &currentBuffer()->layout.formitem()->anchors[i];
      auto f2 = (FormItemList *)a2->url;
      if (f2->parent == fi->parent && f2->name && f2->value &&
          f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN &&
          f2->type != FORM_INPUT_RESET) {
        f2->value = f2->init_value;
        f2->checked = f2->init_checked;
        formUpdateBuffer(a2, &currentBuffer()->layout, f2);
      }
    }
    break;
  case FORM_INPUT_HIDDEN:
  default:
    break;
  }

  return {};
}

std::shared_ptr<Buffer> TabBuffer::followAnchor(bool check_target) {
  if (!this->currentBuffer()->layout.firstLine) {
    return {};
  }

  auto a = this->currentBuffer()->layout.retrieveCurrentAnchor();
  if (!a) {
    return this->_followForm(false);
  }

  if (*a->url == '#') { /* index within this buffer */
    return this->gotoLabel(a->url + 1);
  }

  auto u = urlParse(a->url, this->currentBuffer()->res->getBaseURL());
  if (u.to_Str() == this->currentBuffer()->res->currentURL.to_Str()) {
    /* index within this buffer */
    if (u.label.size()) {
      return this->gotoLabel(u.label.c_str());
    }
  }

  if (check_target && open_tab_blank && a->target &&
      (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
    App::instance()._newT();
    auto buf = this->currentBuffer();
    this->loadLink(a->url, a->target, a->option, nullptr);
    if (buf != this->currentBuffer()) {
      this->deleteBuffer(buf);
      return buf;
    } else {
      App::instance().deleteTab(this);
      return {};
    }
  }

  return this->loadLink(a->url, a->target, a->option, nullptr);
}
