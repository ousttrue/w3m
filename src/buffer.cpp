#include "buffer.h"
#include "url_quote.h"
#include "linein.h"
#include "symbol.h"
#include "file_util.h"
#include "history.h"
#include "quote.h"
#include "app.h"
#include "search.h"
#include "search.h"
#include "tabbuffer.h"
#include "http_response.h"
#include "http_session.h"
#include "w3m.h"
#include "local_cgi.h"
#include "etc.h"
#include "screen.h"
#include "rc.h"
#include "textlist.h"
#include "linklist.h"
#include "html/readbuffer.h"
#include "html/form.h"
#include "html/form_item.h"
#include "html/anchorlist.h"
#include "ctrlcode.h"
#include "line.h"
#include "proto.h"
#include "alloc.h"
#include <sys/stat.h>
#include <sstream>

Buffer::Buffer(const std::shared_ptr<HttpResponse> &_res) : res(_res) {
  // use default from -o mark_all_pages
  if (!res) {
    res = std::make_shared<HttpResponse>();
  }
  this->layout.check_url = MarkAllPages;
}

Buffer::~Buffer() {}

/* append links */
std::string Buffer::link_info() const {
  auto link = this->layout.data.linklist;
  if (!link) {
    return {};
  }

  // LinkList *l;

  std::stringstream html;
  html << "<hr width=50%><h1>Link information</h1><table>\n";

  for (auto l = link; l; l = l->next) {
    Url pu;
    std::string url;
    if (l->url) {
      pu = urlParse(l->url, this->res->getBaseURL());
      url = html_quote(pu.to_Str().c_str());
    } else {
      url = "(empty)";
    }
    html << "<tr valign=top><td><a href=\"" << url << "\">"
         << (l->title ? html_quote(l->title) : "(empty)") << "</a><td>";
    if (l->type == LINK_TYPE_REL) {
      html << "[Rel]";
    } else if (l->type == LINK_TYPE_REV) {
      html << "[Rev]";
    }
    if (!l->url) {
      url = "(empty)";
    } else {
      url = html_quote(url_decode0(l->url));
    }
    html << "<td>" << url;
    if (l->ctype) {
      html << " (" << html_quote(l->ctype) << ")";
    }
    html << "\n";
  }
  html << "</table>\n";
  return html.str();
}

/*
 * information of current page and link
 */
std::shared_ptr<Buffer> Buffer::page_info_panel() {
  Str *tmp = Strnew_size(1024);
  Url pu;
  struct frameset *f_set = nullptr;
  int all;
  const char *p, *q;

  Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");

  all = this->layout.data.lines.size();
  if (all == 0 && this->layout.lastLine())
    all = this->layout.linenumber(this->layout.lastLine());
  p = url_decode0(this->res->currentURL.to_Str().c_str());
  Strcat_m_charp(
      tmp, "<table cellpadding=0>", "<tr valign=top><td nowrap>Title<td>",
      html_quote(this->layout.data.title.c_str()),
      "<tr valign=top><td nowrap>Current URL<td>", html_quote(p),
      "<tr valign=top><td nowrap>Document Type<td>",
      this->res->type.size() ? html_quote(this->res->type.c_str()) : "unknown",
      "<tr valign=top><td nowrap>Last Modified<td>",
      html_quote(this->res->last_modified().c_str()), nullptr);
  Strcat_m_charp(tmp, "<tr valign=top><td nowrap>Number of lines<td>",
                 Sprintf("%d", all)->ptr,
                 "<tr valign=top><td nowrap>Transferred bytes<td>",
                 Sprintf("%lu", (unsigned long)this->res->trbyte)->ptr,
                 nullptr);

  if (auto a = this->layout.retrieveCurrentAnchor()) {
    pu = urlParse(a->url.c_str(), this->res->getBaseURL());
    p = Strnew(pu.to_Str())->ptr;
    q = html_quote(p);
    if (DecodeURL)
      p = html_quote(url_decode0(p));
    else
      p = q;
    Strcat_m_charp(
        tmp, "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"", q,
        "\">", p, "</a>", nullptr);
  }

  if (auto a = this->layout.retrieveCurrentImg()) {
    pu = urlParse(a->url.c_str(), this->res->getBaseURL());
    p = Strnew(pu.to_Str())->ptr;
    q = html_quote(p);
    if (DecodeURL)
      p = html_quote(url_decode0(p));
    else
      p = q;
    Strcat_m_charp(
        tmp, "<tr valign=top><td nowrap>URL of current image<td><a href=\"", q,
        "\">", p, "</a>", nullptr);
  }

  if (auto a = this->layout.retrieveCurrentForm()) {
    FormItemList *fi = a->formItem;
    p = Strnew(fi->form2str())->ptr;
    p = html_quote(url_decode0(p));
    Strcat_m_charp(
        tmp, "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>",
        p, nullptr);
    // if (fi->parent->method == FORM_METHOD_INTERNAL &&
    //     !Strcmp_charp(fi->parent->action, "map"))
    //   append_map_info(buf, tmp, fi->parent->item);
  }

  Strcat_charp(tmp, "</table>\n");

  Strcat(tmp, this->link_info());

  if (this->res->document_header.size()) {
    Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
    for (auto &ti : this->res->document_header) {
      Strcat_m_charp(tmp, "<pre_int>", html_quote(ti.c_str()), "</pre_int>\n",
                     nullptr);
    }
    Strcat_charp(tmp, "</pre>\n");
  }

  if (f_set) {
    Strcat_charp(tmp, "<hr width=50%><h1>Frame information</h1>\n");
    // append_frame_info(buf, tmp, f_set, 0);
  }
  if (this->res->ssl_certificate)
    Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
                   html_quote(this->res->ssl_certificate), "</pre>\n", nullptr);

  Strcat_charp(tmp, "</body></html>");
  auto newbuf = loadHTMLString(tmp->ptr);
  return newbuf;
}

void Buffer::saveBuffer(FILE *f, bool cont) {
  auto is_html = this->res->is_html_type();
  auto l = this->layout.firstLine();
  for (; !layout.isNull(l); ++l) {
    Str *tmp;
    if (is_html)
      tmp = conv_symbol(l);
    else
      tmp = Strnew_charp_n(l->lineBuf.data(), l->lineBuf.size());
    Strfputs(tmp, f);
  }
}

std::shared_ptr<Buffer> link_list_panel(const std::shared_ptr<Buffer> &buf) {
  LinkList *l;
  FormItemList *fi;
  const char *u, *p;
  const char *t;
  Url pu;
  Str *tmp = Strnew_charp("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

  if (buf->layout.data.linklist) {
    Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
    for (l = buf->layout.data.linklist; l; l = l->next) {
      if (l->url) {
        pu = Url::parse(l->url, buf->res->getBaseURL());
        p = Strnew(pu.to_Str())->ptr;
        u = html_quote(p);
        if (DecodeURL)
          p = html_quote(url_decode0(p));
        else
          p = u;
      } else
        u = p = "";
      if (l->type == LINK_TYPE_REL)
        t = " [Rel]";
      else if (l->type == LINK_TYPE_REV)
        t = " [Rev]";
      else
        t = "";
      t = Sprintf("%s%s\n", l->title ? l->title : "", t)->ptr;
      t = html_quote(t);
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  if (buf->layout.data._href) {
    Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
    // auto al = buf->layout.data._href;
    for (size_t i = 0; i < buf->layout.data._href->size(); i++) {
      auto a = &buf->layout.data._href->anchors[i];
      if (a->hseq < 0 || a->slave)
        continue;
      pu = urlParse(a->url.c_str(), buf->res->getBaseURL());
      p = Strnew(pu.to_Str())->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      t = buf->layout.data.getAnchorText(a);
      t = t ? html_quote(t) : "";
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  if (buf->layout.data._img) {
    Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
    auto al = buf->layout.data._img;
    for (size_t i = 0; i < al->size(); i++) {
      auto a = &al->anchors[i];
      if (a->slave)
        continue;
      pu = urlParse(a->url.c_str(), buf->res->getBaseURL());
      p = Strnew(pu.to_Str())->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      if (a->title.size())
        t = html_quote(a->title.c_str());
      else
        t = html_quote(url_decode0(a->url.c_str()));
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
      if (!buf->layout.data._formitem) {
        continue;
      }

      auto fa = buf->layout.data._formitem->retrieveAnchor(a->start.line,
                                                           a->start.pos);
      if (!fa)
        continue;
      fi = fa->formItem;
      fi = fi->parent->item;
      if (fi->parent->method == FORM_METHOD_INTERNAL &&
          !Strcmp_charp(fi->parent->action, "map") && fi->value) {
        // MapList *ml = searchMapList(buf, fi->value->ptr);
        // ListItem *mi;
        // MapArea *m;
        // if (!ml)
        //   continue;
        // Strcat_charp(tmp, "<br>\n<b>Image map</b>\n<ol>\n");
        // for (mi = ml->area->first; mi != NULL; mi = mi->next) {
        //   m = (MapArea *)mi->ptr;
        //   if (!m)
        //     continue;
        //   parseURL2(m->url, &pu, baseURL(buf));
        //   p = Url2Str(&pu)->ptr;
        //   u = html_quote(p);
        //   if (DecodeURL)
        //     p = html_quote(url_decode2(p, buf));
        //   else
        //     p = u;
        //   if (m->alt && *m->alt)
        //     t = html_quote(m->alt);
        //   else
        //     t = html_quote(url_decode2(m->url, buf));
        //   Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
        //                  "\n", NULL);
        // }
        // Strcat_charp(tmp, "</ol>\n");
      }
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  return loadHTMLString(tmp->ptr);
}

void Buffer::saveBufferInfo() {
  if (FILE *fp = fopen(rcFile("bufinfo"), "w")) {
    fprintf(fp, "%s\n", this->res->currentURL.to_Str().c_str());
    fclose(fp);
  }
}

std::shared_ptr<Buffer> Buffer::sourceBuffer() {

  if (this->res->is_html_type()) {
    auto buf = Buffer::create();
    buf->res->type = "text/plain";
    buf->layout.data.title =
        Sprintf("source of %s", this->layout.data.title.c_str())->ptr;

    buf->res->currentURL = this->res->currentURL;
    buf->res->filename = this->res->filename;
    buf->res->sourcefile = this->res->sourcefile;
    buf->layout.data.need_reshape = true;

    return buf;
  } else if (CurrentTab->currentBuffer()->res->type == "text/plain") {
    auto buf = Buffer::create();
    buf->res->type = "text/html";
    buf->layout.data.title =
        Sprintf("HTML view of %s",
                CurrentTab->currentBuffer()->layout.data.title.c_str())
            ->ptr;

    buf->res->currentURL = this->res->currentURL;
    buf->res->filename = this->res->filename;
    buf->res->sourcefile = this->res->sourcefile;
    buf->layout.data.need_reshape = true;

    return buf;
  }

  return {};
}

std::shared_ptr<Buffer> Buffer::gotoLabel(std::string_view label) {
  auto a = this->layout.data._name->searchAnchor(label);
  if (!a) {
    App::instance().disp_message(Sprintf("%s is not found", label)->ptr);
    return {};
  }

  auto buf = Buffer::create();
  *buf = *this;
  buf->res->currentURL.label = label;
  URLHist->push(buf->res->currentURL.to_Str());
  // this->pushBuffer(buf);
  buf->layout.gotoLine(a->start.line);
  if (label_topline)
    buf->layout.topLine = buf->layout.lineSkip(
        buf->layout.topLine, buf->layout.linenumber(buf->layout.currentLine) -
                                 buf->layout.linenumber(buf->layout.topLine));
  buf->layout.pos = a->start.pos;
  buf->layout.arrangeCursor();
  return buf;
}

std::shared_ptr<Buffer> Buffer::loadLink(const char *url, HttpOption option,
                                         FormList *request) {
  App::instance().message(Sprintf("loading %s", url)->ptr, 0, 0);
  // refresh(term_io());

  const int *no_referer_ptr = nullptr;
  auto base = this->res->getBaseURL();

  if ((no_referer_ptr && *no_referer_ptr) || !base ||
      base->schema == SCM_LOCAL || base->schema == SCM_LOCAL_CGI ||
      base->schema == SCM_DATA) {
    option.no_referer = true;
  }
  if (option.referer.empty()) {
    option.referer = this->res->currentURL.to_RefererStr();
  }

  auto res = loadGeneralFile(url, this->res->getBaseURL(), option, request);
  if (!res) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    App::instance().disp_err_message(emsg);
    return {};
  }

  auto buf = Buffer::create(res);

  auto pu = urlParse(url, base);
  URLHist->push(pu.to_Str().c_str());

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
std::shared_ptr<Buffer> Buffer::do_submit(FormItemList *fi, Anchor *a) {
  auto tmp2 = fi->parent->action->Strdup();
  if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
    /* It means "current URL" */
    tmp2 = Strnew(this->res->currentURL.to_Str());
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
    return this->loadLink(tmp2->ptr, {}, nullptr);
    // if (buf) {
    //   App::instance().pushBuffer(buf, a->target);
    // }
    // return buf;
    // if (buf) {
    //   App::instance().pushBuffer(buf);
    // }
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
    auto buf = this->loadLink(tmp2->ptr, {}, fi->parent);
    if (fi->parent->enctype == FORM_ENCTYPE_MULTIPART) {
      unlink(fi->parent->body);
    }
    if (buf &&
        !(buf->res->redirectins.size() > 1)) { /* buf must be Currentbuf */
      /* BP_REDIRECTED means that the buffer is obtained through
       * Location: header. In this case, buf->form_submit must not be set
       * because the page is not loaded by POST method but GET method.
       */
      buf->layout.data.form_submit = save_submit_formlist(fi);
    }
    return buf;
  } else if ((fi->parent->method == FORM_METHOD_INTERNAL &&
              (!Strcmp_charp(fi->parent->action, "map") ||
               !Strcmp_charp(fi->parent->action, "none")))) { /* internal */
    auto tmp = fi->query_from_followform();
    do_internal(tmp2->ptr, tmp->ptr);
    return {};
  } else {
    App::instance().disp_err_message(
        "Can't send form because of illegal method.");
    return {};
  }
}

std::shared_ptr<CoroutineState<std::shared_ptr<Buffer>>>
Buffer::followForm(FormAnchor *a, bool submit) {
  // if (!currentBuffer()->layout.firstLine) {
  //   return {};
  // }

  // auto a = currentBuffer()->layout.retrieveCurrentForm();
  if (!a) {
    co_return {};
  }
  auto fi = a->formItem;

  switch (fi->type) {
  case FORM_INPUT_TEXT: {
    if (submit) {
      co_return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      App::instance().disp_message_nsec("Read only field!", 1, true);
      co_return {};
    }
    auto input = LineInput::inputStrHist(
        App::instance().screen(), "TEXT:", fi->value ? fi->value->ptr : nullptr,
        TextHist);
    input->draw();
    App::instance().pushDispatcher(
        [input](const char *buf, size_t len) -> bool {
          return input->dispatch(buf, len);
        });

    auto p = co_await input;
    if (p.size()) {
      fi->value = Strnew(p);
      formUpdateBuffer(a, &CurrentTab->currentBuffer()->layout, fi);
      if (fi->accept || fi->parent->nitems == 1) {
        co_return CurrentTab->currentBuffer()->do_submit(fi, a);
      }
    }

    co_return {};
  } break;

  case FORM_INPUT_FILE:
    if (submit) {
      co_return this->do_submit(fi, a);
    }

    if (fi->readonly) {
      App::instance().disp_message_nsec("Read only field!", 1, true);
    }

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
      co_return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      App::instance().disp_message_nsec("Read only field!", 1, true);
      break;
    }
    // p = inputLine("Password:", fi->value ? fi->value->ptr : nullptr,
    // IN_PASSWORD);
    // if (p == nullptr)
    //   break;
    // fi->value = Strnew_charp(p);
    formUpdateBuffer(a, &this->layout, fi);
    if (fi->accept) {
      co_return this->do_submit(fi, a);
    }
    break;

  case FORM_TEXTAREA:
    if (submit) {
      co_return this->do_submit(fi, a);
    }
    if (fi->readonly)
      App::instance().disp_message_nsec("Read only field!", 1, true);
    input_textarea(fi);
    formUpdateBuffer(a, &this->layout, fi);
    break;

  case FORM_INPUT_RADIO:
    if (submit) {
      co_return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      App::instance().disp_message_nsec("Read only field!", 1, true);
      break;
    }
    formRecheckRadio(a, this, fi);
    break;

  case FORM_INPUT_CHECKBOX:
    if (submit) {
      co_return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      App::instance().disp_message_nsec("Read only field!", 1, true);
      break;
    }
    fi->checked = !fi->checked;
    formUpdateBuffer(a, &this->layout, fi);
    break;
  case FORM_INPUT_IMAGE:
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
    co_return this->do_submit(fi, a);

  case FORM_INPUT_RESET:
    for (size_t i = 0; i < this->layout.data._formitem->size(); i++) {
      auto a2 = &this->layout.data._formitem->anchors[i];
      auto f2 = a2->formItem;
      if (f2->parent == fi->parent && f2->name && f2->value &&
          f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN &&
          f2->type != FORM_INPUT_RESET) {
        f2->value = f2->init_value;
        f2->checked = f2->init_checked;
        formUpdateBuffer(a2, &this->layout, f2);
      }
    }
    break;

  case FORM_INPUT_HIDDEN:
  default:
    break;
  }

  co_return {};
}

std::shared_ptr<CoroutineState<std::tuple<Anchor *, std::shared_ptr<Buffer>>>>
Buffer::followAnchor(bool check_target) {
  if (this->layout.empty()) {
    co_return {};
  }

  if (auto a = this->layout.retrieveCurrentAnchor()) {
    if (a->url.size() && a->url[0] == '#') { /* index within this buffer */
      co_return {a, this->gotoLabel(a->url.substr(1))};
    }

    auto u = urlParse(a->url.c_str(), this->res->getBaseURL());
    if (u.to_Str() == this->res->currentURL.to_Str()) {
      // index within this buffer
      if (u.label.size()) {
        co_return {a, this->gotoLabel(u.label.c_str())};
      }
    }

    co_return {a, this->loadLink(a->url.c_str(), a->option, nullptr)};
  }

  if (auto f = this->layout.retrieveCurrentForm()) {
    co_return {f, co_await this->followForm(f, false)};
  }

  co_return {};
}

std::shared_ptr<Buffer> Buffer::cmd_loadURL(const char *url,
                                            std::optional<Url> current,
                                            const HttpOption &option,
                                            FormList *request) {
  auto res = loadGeneralFile(url, current, option, request);
  if (!res) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    App::instance().disp_err_message(emsg);
    return {};
  }

  return Buffer::create(res);
}

std::shared_ptr<Buffer> Buffer::goURL0(const char *url, const char *prompt,
                                       bool relative) {
  std::optional<Url> current;
  if (!url) {
    auto hist = URLHist->copyHist();

    current = this->res->getBaseURL();
    if (current) {
      auto c_url = current->to_Str();
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url.c_str());
      else
        hist->push(c_url);
    }
    auto a = this->layout.retrieveCurrentAnchor();
    if (a) {
      auto p_url = urlParse(a->url.c_str(), current);
      auto a_url = p_url.to_Str();
      if (DefaultURLString == DEFAULT_URL_LINK)
        url = url_decode0(a_url.c_str());
      else
        hist->push(a_url);
    }
    // url = inputLineHist(prompt, url, IN_URL, hist);
    if (url != nullptr) {
      SKIP_BLANKS(url);
    }
  }

  HttpOption option = {};
  if (relative) {
    const int *no_referer_ptr = nullptr;
    current = this->res->getBaseURL();
    if ((no_referer_ptr && *no_referer_ptr) || !current ||
        current->schema == SCM_LOCAL || current->schema == SCM_LOCAL_CGI ||
        current->schema == SCM_DATA)
      option.no_referer = true;
    else
      option.referer = this->res->currentURL.to_RefererStr();
    url = Strnew(url_quote(url))->ptr;
  } else {
    current = {};
    url = Strnew(url_quote(url))->ptr;
  }

  if (url == nullptr || *url == '\0') {
    return {};
  }

  if (*url == '#') {
    return this->gotoLabel(url + 1);
  }

  auto p_url = urlParse(url, current);
  URLHist->push(p_url.to_Str());
  return this->cmd_loadURL(url, current, option, nullptr);
}
