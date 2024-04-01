#include "buffer.h"
#include "url_quote.h"
#include "linein.h"
#include "symbol.h"
#include "url_decode.h"
#include "quote.h"
#include "search.h"
#include "search.h"
#include "tabbuffer.h"
#include "http_response.h"
#include "http_session.h"
#include "local_cgi.h"
#include "rc.h"
#include "anchorlist.h"
#include "ctrlcode.h"
#include "line.h"
#include "history.h"
#include "cookie.h"
#include "downloadlist.h"
#include "app.h"

#include <sys/stat.h>
#include <sstream>
#include <plog/Log.h>

DefaultUrlStringType DefaultURLString = DEFAULT_URL_CURRENT;

Buffer::Buffer(const std::shared_ptr<HttpResponse> &_res)
    : res(_res), layout(new LineLayout) {
  // use default from -o mark_all_pages
  if (!res) {
    res = std::make_shared<HttpResponse>();
  }
  // this->layout->check_url = MarkAllPages;
}

Buffer::~Buffer() {}

std::shared_ptr<Buffer> Buffer::fromHtml(const std::string &html) {
  auto res = HttpResponse::fromHtml(html);
  return std::shared_ptr<Buffer>(new Buffer(res));
}

/* append links */
std::string Buffer::link_info() const {
  std::stringstream html;
  html << "<hr width=50%><h1>Link information</h1><table>\n";

  for (auto &l : this->layout->data.linklist) {
    Url pu;
    std::string url;
    if (l.url.size()) {
      pu = {l.url, this->layout->data.baseURL};
      url = html_quote(pu.to_Str().c_str());
    } else {
      url = "(empty)";
    }
    html << "<tr valign=top><td><a href=\"" << url << "\">"
         << (l.title.size() ? html_quote(l.title) : "(empty)") << "</a><td>";
    if (l.type == LINK_TYPE_REL) {
      html << "[Rel]";
    } else if (l.type == LINK_TYPE_REV) {
      html << "[Rev]";
    }
    if (l.url.empty()) {
      url = "(empty)";
    } else {
      url = html_quote(url_decode0(l.url));
    }
    html << "<td>" << url;
    if (l.ctype.size()) {
      html << " (" << html_quote(l.ctype) << ")";
    }
    html << "\n";
  }
  html << "</table>\n";
  return html.str();
}

/*
 * information of current page and link
 */
std::string Buffer::page_info_panel() const {
  std::stringstream tmp;

  {
    tmp << "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n";

    int all = this->layout->data.lines.size();
    if (all == 0 && this->layout->lastLine())
      all = this->layout->linenumber(this->layout->lastLine());
    auto p = url_decode0(this->res->currentURL.to_Str());
    tmp << "<table cellpadding=0>"
        << "<tr valign=top><td nowrap>Title<td>"
        << html_quote(this->layout->data.title.c_str())
        << "<tr valign=top><td nowrap>Current URL<td>" << html_quote(p)
        << "<tr valign=top><td nowrap>Document Type<td>"
        << (this->res->type.size() ? html_quote(this->res->type.c_str())
                                   : "unknown")
        << "<tr valign=top><td nowrap>Last Modified<td>"
        << html_quote(this->res->last_modified().c_str());
    // Strcat_m_charp(tmp, "<tr valign=top><td nowrap>Number of lines<td>",
    //                Sprintf("%d", all)->ptr,
    //                "<tr valign=top><td nowrap>Transferred bytes<td>",
    //                Sprintf("%lu", (unsigned long)this->res->trbyte)->ptr,
    //                nullptr);
  }

  if (auto a = this->layout->retrieveCurrentAnchor()) {
    Url pu(a->url.c_str(), this->layout->data.baseURL);
    auto p = pu.to_Str();
    auto q = html_quote(p);
    if (DecodeURL)
      p = html_quote(url_decode0(p));
    else
      p = q;
    tmp << "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"" << q
        << "\">" << p << "</a>";
  }

  if (auto a = this->layout->retrieveCurrentImg()) {
    Url pu(a->url.c_str(), this->layout->data.baseURL);
    auto p = pu.to_Str();
    auto q = html_quote(p);
    if (DecodeURL)
      p = html_quote(url_decode0(p));
    else
      p = q;
    tmp << "<tr valign=top><td nowrap>URL of current image<td><a href=\"" << q
        << "\">" << p << "</a>";
  }

  if (auto a = this->layout->retrieveCurrentForm()) {
    auto fi = a->formItem;
    auto p = html_quote(url_decode0(fi->form2str()));
    tmp << "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>"
        << p;
    // if (fi->parent->method == FORM_METHOD_INTERNAL &&
    //     !Strcmp_charp(fi->parent->action, "map"))
    //   append_map_info(buf, tmp, fi->parent->item);
  }

  tmp << "</table>\n";

  tmp << this->link_info();

  if (this->res->document_header.size()) {
    tmp << "<hr width=50%><h1>Header information</h1><pre>\n";
    for (auto &ti : this->res->document_header) {
      tmp << "<pre_int>" << html_quote(ti) << "</pre_int>\n";
    }
    tmp << "</pre>\n";
  }

  // if (f_set) {
  //   Strcat_charp(tmp, "<hr width=50%><h1>Frame information</h1>\n");
  //   // append_frame_info(buf, tmp, f_set, 0);
  // }
  if (this->res->ssl_certificate)
    tmp << "<h1>SSL certificate</h1><pre>\n"
        << html_quote(this->res->ssl_certificate) << "</pre>\n";

  tmp << "</body></html>";
  return tmp.str();
}

void Buffer::saveBuffer(FILE *f, bool cont) {
  auto is_html = this->res->is_html_type();
  auto l = this->layout->firstLine();
  for (; !layout->isNull(l); ++l) {
    std::string tmp;
    if (is_html)
      tmp = conv_symbol(l);
    else
      tmp = std::string(l->lineBuf(), l->len());
    fwrite(tmp.c_str(), 1, tmp.size(), f);
  }
}

std::string link_list_panel(const std::shared_ptr<Buffer> &buf) {
  std::stringstream tmp;
  tmp << "<title>Link List</title>\
<h1 align=center>Link List</h1>\n";

  if (buf->layout->data.linklist.size()) {
    tmp << "<hr><h2>Links</h2>\n<ol>\n";
    for (auto &l : buf->layout->data.linklist) {
      std::string p;
      std::string u;
      if (l.url.size()) {
        Url pu(l.url, buf->layout->data.baseURL);
        p = pu.to_Str();
        u = html_quote(p);
        if (DecodeURL)
          p = html_quote(url_decode0(p));
        else
          p = u;
      } else
        u = p = "";

      std::string t = "";
      if (l.type == LINK_TYPE_REL)
        t = " [Rel]";
      else if (l.type == LINK_TYPE_REV)
        t = " [Rev]";

      std::stringstream ss;
      ss << l.title << t << "\n";
      t = html_quote(ss.str());
      tmp << "<li><a href=\"" << u << "\">" << t << "</a><br>" << p << "\n";
    }
    tmp << "</ol>\n";
  }

  if (buf->layout->data._href) {
    tmp << "<hr><h2>Anchors</h2>\n<ol>\n";
    // auto al = buf->layout.data._href;
    for (size_t i = 0; i < buf->layout->data._href->size(); i++) {
      auto a = &buf->layout->data._href->anchors[i];
      if (a->hseq < 0 || a->slave)
        continue;
      Url pu(a->url.c_str(), buf->layout->data.baseURL);
      auto p = pu.to_Str();
      auto u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      auto t = html_quote(buf->layout->data.getAnchorText(a));
      tmp << "<li><a href=\"" << u << "\">" << t << "</a><br>" << p << "\n";
    }
    tmp << "</ol>\n";
  }

  if (buf->layout->data._img) {
    tmp << "<hr><h2>Images</h2>\n<ol>\n";
    auto al = buf->layout->data._img;
    for (size_t i = 0; i < al->size(); i++) {
      auto a = &al->anchors[i];
      if (a->slave)
        continue;
      Url pu(a->url.c_str(), buf->layout->data.baseURL);
      auto p = pu.to_Str();
      auto u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      std::string t;
      if (a->title.size())
        t = html_quote(a->title.c_str());
      else
        t = html_quote(url_decode0(a->url.c_str()));
      tmp << "<li><a href=\"" << u << "\">" << t << "</a><br>" << p << "\n";
      if (!buf->layout->data._formitem) {
        continue;
      }

      auto fa = buf->layout->data._formitem->retrieveAnchor(a->start);
      if (!fa)
        continue;
      // first item
      auto fi = fa->formItem->parent->items.front();
      if (fi->parent->method == FORM_METHOD_INTERNAL &&
          fi->parent->action == "map" && fi->value.size()) {
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
    tmp << "</ol>\n";
  }

  return tmp.str();
}

void Buffer::saveBufferInfo() {
  if (FILE *fp = fopen(rcFile("bufinfo").c_str(), "w")) {
    fprintf(fp, "%s\n", this->res->currentURL.to_Str().c_str());
    fclose(fp);
  }
}

std::shared_ptr<Buffer> Buffer::sourceBuffer() {
  if (this->res->is_html_type()) {
    auto buf = Buffer::create();
    buf->res->type = "text/plain";
    buf->layout->data.title =
        std::string("source of ") + this->layout->data.title;
    buf->res->currentURL = this->res->currentURL;
    buf->res->filename = this->res->filename;
    buf->res->sourcefile = this->res->sourcefile;
    buf->layout->data.need_reshape = true;
    return buf;
  }

  if (this->res->type == "text/plain") {
    auto buf = Buffer::create();
    buf->res->type = "text/html";
    buf->layout->data.title =
        std::string("HTML view of ") + this->layout->data.title;
    buf->res->currentURL = this->res->currentURL;
    buf->res->filename = this->res->filename;
    buf->res->sourcefile = this->res->sourcefile;
    buf->layout->data.need_reshape = true;
    return buf;
  }

  return {};
}

std::shared_ptr<Buffer> Buffer::gotoLabel(const std::string &label) {
  auto a = this->layout->data._name->searchAnchor(label);
  if (!a) {
    PLOGE << label << ": not found";
    return {};
  }

  auto buf = Buffer::create();
  *buf = *this;
  buf->res->currentURL.label = label;
  URLHist->push(buf->res->currentURL.to_Str());
  // this->pushBuffer(buf);
  buf->layout->visual.cursorRow(a->start.line);
  // if (label_topline) {
  //   buf->layout._topLine += buf->layout.cursor.row;
  // }
  buf->layout->cursorPos(a->start.pos);
  return buf;
}

std::shared_ptr<Buffer> Buffer::loadLink(const char *url, HttpOption option,
                                         const std::shared_ptr<Form> &request) {
  PLOGI << "loading: " << url;
  // refresh(term_io());

  const int *no_referer_ptr = nullptr;
  auto base = this->layout->data.baseURL;

  if ((no_referer_ptr && *no_referer_ptr) || base.scheme == SCM_LOCAL ||
      base.scheme == SCM_LOCAL_CGI || base.scheme == SCM_DATA) {
    option.no_referer = true;
  }
  if (option.referer.empty()) {
    option.referer = this->res->currentURL.to_RefererStr();
  }

  auto res = loadGeneralFile(url, this->layout->data.baseURL, option, request);
  if (!res) {
    PLOGE << "Can't load: " << url;
    return {};
  }

  auto buf = Buffer::create(res);

  Url pu(url, base);
  URLHist->push(pu.to_Str().c_str());

  return buf;
}

static std::shared_ptr<FormItem>
save_submit_formlist(const std::shared_ptr<FormItem> &src) {
  if (!src) {
    return nullptr;
  }
  auto srclist = src->parent;
  auto list = std::make_shared<Form>();
  list->method = srclist->method;
  list->action = srclist->action;
  list->enctype = srclist->enctype;
  list->body = srclist->body;
  list->boundary = srclist->boundary;
  list->length = srclist->length;

  std::shared_ptr<FormItem> ret;
  for (auto &srcitem : srclist->items) {
    auto item = std::make_shared<FormItem>();
    item->type = srcitem->type;
    item->name = srcitem->name;
    item->value = srcitem->value;
    item->checked = srcitem->checked;
    item->accept = srcitem->accept;
    item->size = srcitem->size;
    item->rows = srcitem->rows;
    item->maxlength = srcitem->maxlength;
    item->readonly = srcitem->readonly;
    item->parent = list;
    if (srcitem == src) {
      ret = item;
    }
  }

  return ret;
}

std::list<std::pair<std::string, std::string>>
cgistr2tagarg(const char *cgistr) {
  std::list<std::pair<std::string, std::string>> t;
  do {
    std::stringstream tmp;
    while (*cgistr && *cgistr != '=' && *cgistr != '&')
      tmp << *cgistr++;
    auto tag = tmp.str();
    t.push_back({tag, {}});
    if (*cgistr == '\0')
      return t;
    else if (*cgistr == '=') {
      cgistr++;
      std::stringstream value;
      while (*cgistr && *cgistr != '&')
        value << *cgistr++;
      t.back() = {tag, Str_form_unquote(value.str())};
    } else if (*cgistr == '&') {
      cgistr++;
    }
  } while (*cgistr);
  return t;
}

using Action =
    std::function<void(const std::list<std::pair<std::string, std::string>> &)>;

std::unordered_map<std::string, Action> internal_action = {
    {"option", panel_set_option},
    {"cookie", set_cookie_flag},
    {"download", download_action},
    {"none", {}},
};

static void do_internal(const std::string &action, const char *data) {
  auto found = internal_action.find(action);
  if (found != internal_action.end()) {
    if (found->second) {
      found->second(cgistr2tagarg(data));
    }
  }
}

std::shared_ptr<Buffer> Buffer::do_submit(const std::shared_ptr<FormItem> &fi,
                                          Anchor *a) {
  auto form = fi->parent;
  auto tmp2 = form->action;
  if (tmp2 == "!CURRENT_URL!") {
    /* It means "current URL" */
    tmp2 = this->res->currentURL.to_Str();
    // char *p;
    // if ((p = strchr(tmp2->ptr, '?')) != nullptr)
    //   Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
  }

  if (form->method == FORM_METHOD_GET) {
    std::string tmp = fi->parent->query(fi);
    // char *p;
    // if ((p = strchr(tmp2->ptr, '?')) != nullptr)
    //   Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    auto found = tmp2.find('?');
    if (found != std::string::npos) {
      tmp2 = tmp2.substr(0, found);
    }
    tmp2 += "?";
    tmp2 += tmp;
    return this->loadLink(tmp2.c_str(), {}, nullptr);
    // if (buf) {
    //   App::instance().pushBuffer(buf, a->target);
    // }
    // return buf;
    // if (buf) {
    //   App::instance().pushBuffer(buf);
    // }
  } else if (form->method == FORM_METHOD_POST) {
    if (form->enctype == FORM_ENCTYPE_MULTIPART) {
      fi->query_from_followform_multipart(App::instance().pid());
      struct stat st;
      stat(form->body.c_str(), &st);
      form->length = st.st_size;
    } else {
      auto tmp = fi->parent->query(fi);
      form->body = tmp;
      form->length = tmp.size();
    }
    auto buf = this->loadLink(tmp2.c_str(), {}, form);
    if (form->enctype == FORM_ENCTYPE_MULTIPART) {
      unlink(form->body.c_str());
    }
    if (buf &&
        !(buf->res->redirectins.size() > 1)) { /* buf must be Currentbuf */
      /* BP_REDIRECTED means that the buffer is obtained through
       * Location: header. In this case, buf->form_submit must not be set
       * because the page is not loaded by POST method but GET method.
       */
      buf->layout->data.form_submit = save_submit_formlist(fi);
    }
    return buf;
  } else if ((form->method == FORM_METHOD_INTERNAL &&
              (form->action == "map" ||
               form->action == "none"))) { /* internal */
    auto tmp = fi->parent->query(fi);
    do_internal(tmp2.c_str(), tmp.c_str());
    return {};
  } else {
    App::instance().disp_err_message(
        "Can't send form because of illegal method.");
    return {};
  }
}

std::shared_ptr<CoroutineState<std::shared_ptr<Buffer>>>
Buffer::followForm(FormAnchor *a, bool submit) {
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
    auto input = LineInput::inputStrHist("TEXT:", fi->value, TextHist);
    input->draw();
    App::instance().pushDispatcher(
        [input](const char *buf, size_t len) -> bool {
          return input->dispatch(buf, len);
        });

    auto p = co_await input;
    if (p.size()) {
      fi->value = p;
      this->layout->formUpdateBuffer(a);
      this->layout->data.need_reshape = true;
      auto form = fi->parent;
      if (fi->accept || form->items.size() == 1) {
        co_return App::instance().currentTab()->currentBuffer()->do_submit(fi, a);
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
    this->layout->formUpdateBuffer(a);
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
    input_textarea(fi.get());
    this->layout->formUpdateBuffer(a);
    break;

  case FORM_INPUT_RADIO:
    if (submit) {
      co_return this->do_submit(fi, a);
    }
    if (fi->readonly) {
      App::instance().disp_message_nsec("Read only field!", 1, true);
      break;
    }
    this->formRecheckRadio(a);
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
    this->layout->formUpdateBuffer(a);
    break;

  case FORM_INPUT_IMAGE:
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
    co_return this->do_submit(fi, a);

  case FORM_INPUT_RESET:
    for (size_t i = 0; i < this->layout->data._formitem->size(); i++) {
      auto a2 = &this->layout->data._formitem->anchors[i];
      auto f2 = a2->formItem;
      if (f2->parent == fi->parent && f2->name.size() && f2->value.size() &&
          f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN &&
          f2->type != FORM_INPUT_RESET) {
        f2->value = f2->init_value;
        f2->checked = f2->init_checked;
        this->layout->formUpdateBuffer(a2);
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
  if (auto a = this->layout->retrieveCurrentAnchor()) {
    if (a->url.size() && a->url[0] == '#') { /* index within this buffer */
      co_return {a, this->gotoLabel(a->url.substr(1))};
    }

    Url u(a->url.c_str(), this->layout->data.baseURL);
    if (u.to_Str() == this->res->currentURL.to_Str()) {
      // index within this buffer
      if (u.label.size()) {
        co_return {a, this->gotoLabel(u.label.c_str())};
      }
    }

    co_return {a, this->loadLink(a->url.c_str(), a->option, nullptr)};
  }

  if (auto f = this->layout->retrieveCurrentForm()) {
    co_return {f, co_await this->followForm(f, false)};
  }

  co_return {};
}

std::shared_ptr<Buffer>
Buffer::cmd_loadURL(const char *url, std::optional<Url> current,
                    const HttpOption &option,
                    const std::shared_ptr<Form> &request) {
  auto res = loadGeneralFile(url, current, option, request);
  if (!res) {
    auto emsg = std::string("Can't load ") + url;
    App::instance().disp_err_message(emsg);
    return {};
  }

  return Buffer::create(res);
}

std::shared_ptr<Buffer> Buffer::goURL0(std::string_view _url,
                                       const char *prompt, bool relative) {
  std::optional<Url> current;
  std::string url;
  if (_url.empty()) {
    auto hist = URLHist->copyHist();

    current = this->layout->data.baseURL;
    if (current) {
      auto c_url = current->to_Str();
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url);
      else
        hist->push(c_url);
    }
    auto a = this->layout->retrieveCurrentAnchor();
    if (a) {
      Url p_url(a->url.c_str(), current);
      auto a_url = p_url.to_Str();
      if (DefaultURLString == DEFAULT_URL_LINK)
        url = url_decode0(a_url.c_str());
      else
        hist->push(a_url);
    }
    // url = inputLineHist(prompt, url, IN_URL, hist);
    // if (url != nullptr) {
    //   SKIP_BLANKS(url);
    // }
  }

  HttpOption option = {};
  if (relative) {
    const int *no_referer_ptr = nullptr;
    current = this->layout->data.baseURL;
    if ((no_referer_ptr && *no_referer_ptr) || !current ||
        current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI ||
        current->scheme == SCM_DATA)
      option.no_referer = true;
    else
      option.referer = this->res->currentURL.to_RefererStr();
    url = url_quote(url);
  } else {
    current = {};
    url = url_quote(url);
  }

  if (url.empty()) {
    return {};
  }

  if (url[0] == '#') {
    return this->gotoLabel(url.substr(1));
  }

  Url p_url(url.c_str(), current);
  URLHist->push(p_url.to_Str());
  return this->cmd_loadURL(url.c_str(), current, option, nullptr);
}

void Buffer::formRecheckRadio(FormAnchor *a) {
  auto fi = a->formItem;
  for (size_t i = 0; i < this->layout->data._formitem->size(); i++) {
    auto a2 = &this->layout->data._formitem->anchors[i];
    auto f2 = a2->formItem;
    if (f2->parent == fi->parent && f2 != fi && f2->type == FORM_INPUT_RADIO &&
        f2->name == fi->name) {
      f2->checked = 0;
      this->layout->formUpdateBuffer(a2);
    }
  }
  fi->checked = 1;
  this->layout->formUpdateBuffer(a);
}
