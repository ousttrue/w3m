#include "tabbuffer.h"
#include "html/form_item.h"
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
    if (auto buf = this->currentBuffer()->gotoLabel(url + 1)) {
      this->pushBuffer(buf);
    }
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
      auto buf = this->currentBuffer()->do_submit(fi, a);
      if (buf) {
        App::instance().pushBuffer(buf, a->target);
      }
      return buf;
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
                     auto buf = CurrentTab->currentBuffer()->do_submit(fi, a);
                     if (buf) {
                       App::instance().pushBuffer(buf, a->target);
                     }
                     return;
                   }
                   App::instance().invalidate();
                 });
    break;

  case FORM_INPUT_FILE:
    if (submit) {
      auto buf = this->currentBuffer()->do_submit(fi, a);
      if (buf) {
        App::instance().pushBuffer(buf, a->target);
      }
      return buf;
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
      auto buf = this->currentBuffer()->do_submit(fi, a);
      if (buf) {
        App::instance().pushBuffer(buf, a->target);
      }
      return buf;
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
      auto buf = this->currentBuffer()->do_submit(fi, a);
      if (buf) {
        App::instance().pushBuffer(buf, a->target);
      }
      return buf;
    }
    break;
  case FORM_TEXTAREA:
    if (submit) {
      auto buf = this->currentBuffer()->do_submit(fi, a);
      if (buf) {
        App::instance().pushBuffer(buf, a->target);
      }
      return buf;
    }
    if (fi->readonly)
      disp_message_nsec("Read only field!", 1, true);
    input_textarea(fi);
    formUpdateBuffer(a, &currentBuffer()->layout, fi);
    break;
  case FORM_INPUT_RADIO:
    if (submit) {
      auto buf = this->currentBuffer()->do_submit(fi, a);
      if (buf) {
        App::instance().pushBuffer(buf, a->target);
      }
      return buf;
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", 1, true);
      break;
    }
    formRecheckRadio(a, currentBuffer(), fi);
    break;
  case FORM_INPUT_CHECKBOX:
    if (submit) {
      auto buf = this->currentBuffer()->do_submit(fi, a);
      if (buf) {
        App::instance().pushBuffer(buf, a->target);
      }
      return buf;
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
    this->currentBuffer()->do_submit(fi, a);
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
    auto buf = this->currentBuffer()->gotoLabel(a->url + 1);
    if (buf) {
      this->pushBuffer(buf);
    }
    return buf;
  }

  auto u = urlParse(a->url, this->currentBuffer()->res->getBaseURL());
  if (u.to_Str() == this->currentBuffer()->res->currentURL.to_Str()) {
    // index within this buffer
    if (u.label.size()) {
      auto buf = this->currentBuffer()->gotoLabel(u.label.c_str());
      if (buf) {
        this->pushBuffer(buf);
      }
      return buf;
    }
  }

  auto buf = this->currentBuffer()->loadLink(a->url, a->option, nullptr);
  if (!buf) {
    return {};
  }
  App::instance().pushBuffer(buf, a->target);

  if (check_target && open_tab_blank && a->target.size() &&
      (a->target == "_new" || a->target == "_blank")) {
    // open in new tab
    App::instance().newTab(buf);
  } else {
    this->pushBuffer(buf);
  }
  return buf;
}
