#include "buffer.h"
#include "symbol.h"
#include "file_util.h"
#include "history.h"
#include "quote.h"
#include "app.h"
#include "search.h"
#include "signal_util.h"
#include "search.h"
#include "tabbuffer.h"
#include "http_response.h"
#include "http_session.h"
#include "w3m.h"
#include "local_cgi.h"
#include "etc.h"
#include "url_stream.h"
#include "message.h"
#include "screen.h"
#include "display.h"
#include "rc.h"
#include "textlist.h"
#include "linklist.h"
#include "terms.h"
#include "html/form_item.h"
#include "html/readbuffer.h"
#include "html/form.h"
#include "html/anchor.h"
#include "ctrlcode.h"
#include "line.h"
#include "istream.h"
#include "proto.h"
#include <unistd.h>

Buffer::Buffer(const std::shared_ptr<HttpResponse> &_res) : res(_res) {
  // use default from -o mark_all_pages
  if (!res) {
    res = std::make_shared<HttpResponse>();
  }
  this->check_url = MarkAllPages;
}

Buffer::~Buffer() {}

/*
 * Create null buffer
 */
std::shared_ptr<Buffer> nullBuffer(void) {
  auto b = Buffer::create();
  b->layout.title = "*Null*";
  return b;
}

std::shared_ptr<Buffer> nthBuffer(const std::shared_ptr<Buffer> &firstbuf,
                                  int n) {
  int i;
  std::shared_ptr<Buffer> buf;

  if (n < 0)
    return firstbuf;
  for (i = 0; i < n; i++) {
    if (buf == nullptr)
      return nullptr;
    buf = buf->backBuffer;
  }
  return buf;
}

static void writeBufferName(const std::shared_ptr<Buffer> &buf, int n) {
  int all = buf->layout.allLine;
  if (all == 0 && buf->layout.lastLine != nullptr) {
    all = buf->layout.lastLine->linenumber;
  }

  move(n, 0);
  auto msg = Sprintf("<%s> [%d lines]", buf->layout.title.c_str(), all);
  if (buf->res->filename.size()) {
    switch (buf->res->currentURL.schema) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
      if (buf->res->currentURL.file != "-") {
        Strcat_char(msg, ' ');
        Strcat(msg, buf->res->currentURL.real_file);
      }
      break;
    case SCM_UNKNOWN:
    case SCM_MISSING:
      break;
    default:
      Strcat_char(msg, ' ');
      Strcat(msg, buf->res->currentURL.to_Str());
      break;
    }
  }
  addnstr_sup(msg->ptr, COLS - 1);
}

static std::shared_ptr<Buffer>
listBuffer(const std::shared_ptr<Buffer> &top,
           const std::shared_ptr<Buffer> &current) {
  int i, c = 0;
  std::shared_ptr<Buffer> buf = top;

  move(0, 0);
  clrtobotx();
  for (i = 0; i < LASTLINE; i++) {
    if (buf == current) {
      c = i;
      standout();
    }
    writeBufferName(buf, i);
    if (buf == current) {
      standend();
      clrtoeolx();
      move(i, 0);
      toggle_stand();
    } else
      clrtoeolx();
    if (buf->backBuffer == nullptr) {
      move(i + 1, 0);
      clrtobotx();
      break;
    }
    buf = buf->backBuffer;
  }
  standout();
  /* FIXME: gettextize? */
  message("Buffer selection mode: SPC for select / D for delete buffer", 0, 0);
  standend();
  /*
   * move(LASTLINE, COLS - 1); */
  move(c, 0);
  refresh(term_io());
  return buf->backBuffer;
}

/*
 * Select buffer visually
 */
std::shared_ptr<Buffer> selectBuffer(const std::shared_ptr<Buffer> &firstbuf,
                                     std::shared_ptr<Buffer> currentbuf,
                                     char *selectchar) {
  int i, cpoint,                  /* Current Buffer Number */
      spoint,                     /* Current Line on Screen */
      maxbuf, sclimit = LASTLINE; /* Upper limit of line * number in
                                   * the * screen */
  std::shared_ptr<Buffer> buf, topbuf;
  char c;

  i = cpoint = 0;
  for (buf = firstbuf; buf != nullptr; buf = buf->backBuffer) {
    if (buf == currentbuf)
      cpoint = i;
    i++;
  }
  maxbuf = i;

  if (cpoint >= sclimit) {
    spoint = sclimit / 2;
    topbuf = nthBuffer(firstbuf, cpoint - spoint);
  } else {
    topbuf = firstbuf;
    spoint = cpoint;
  }
  listBuffer(topbuf, currentbuf);

  for (;;) {
    if ((c = getch()) == ESC_CODE) {
      if ((c = getch()) == '[' || c == 'O') {
        switch (c = getch()) {
        case 'A':
          c = 'k';
          break;
        case 'B':
          c = 'j';
          break;
        case 'C':
          c = ' ';
          break;
        case 'D':
          c = 'B';
          break;
        }
      }
    }
    switch (c) {
    case CTRL_N:
    case 'j':
      if (spoint < sclimit - 1) {
        if (currentbuf->backBuffer == nullptr)
          continue;
        writeBufferName(currentbuf, spoint);
        currentbuf = currentbuf->backBuffer;
        cpoint++;
        spoint++;
        standout();
        writeBufferName(currentbuf, spoint);
        standend();
        move(spoint, 0);
        toggle_stand();
      } else if (cpoint < maxbuf - 1) {
        topbuf = currentbuf;
        currentbuf = currentbuf->backBuffer;
        cpoint++;
        spoint = 1;
        listBuffer(topbuf, currentbuf);
      }
      break;
    case CTRL_P:
    case 'k':
      if (spoint > 0) {
        writeBufferName(currentbuf, spoint);
        currentbuf = nthBuffer(topbuf, --spoint);
        cpoint--;
        standout();
        writeBufferName(currentbuf, spoint);
        standend();
        move(spoint, 0);
        toggle_stand();
      } else if (cpoint > 0) {
        i = cpoint - sclimit;
        if (i < 0)
          i = 0;
        cpoint--;
        spoint = cpoint - i;
        currentbuf = nthBuffer(firstbuf, cpoint);
        topbuf = nthBuffer(firstbuf, i);
        listBuffer(topbuf, currentbuf);
      }
      break;
    default:
      *selectchar = c;
      return currentbuf;
    }
    /*
     * move(LASTLINE, COLS - 1);
     */
    move(spoint, 0);
    refresh(term_io());
  }
}

/*
 * Reshape HTML buffer
 */
void reshapeBuffer(const std::shared_ptr<Buffer> &buf) {
  if (!buf->layout.need_reshape) {
    return;
  }
  buf->layout.need_reshape = false;
  buf->layout.width = INIT_BUFFER_WIDTH();
  if (buf->res->sourcefile.empty()) {
    return;
  }
  buf->res->f.openFile(buf->res->sourcefile.c_str());
  if (buf->res->f.stream == nullptr) {
    return;
  }

  auto sbuf = Buffer::create(0);
  *sbuf = *buf;
  buf->layout.clearBuffer();

  if (buf->res->is_html_type())
    loadHTMLstream(buf->res.get(), &buf->layout);
  else
    loadBuffer(buf->res.get(), &buf->layout);

  buf->layout.height = LASTLINE + 1;
  if (buf->layout.firstLine && sbuf->layout.firstLine) {
    Line *cur = sbuf->layout.currentLine;
    int n;

    buf->layout.pos = sbuf->layout.pos + cur->bpos;
    while (cur->bpos && cur->prev)
      cur = cur->prev;
    if (cur->real_linenumber > 0) {
      buf->layout.gotoRealLine(cur->real_linenumber);
    } else {
      buf->layout.gotoLine(cur->linenumber);
    }
    n = (buf->layout.currentLine->linenumber -
         buf->layout.topLine->linenumber) -
        (cur->linenumber - sbuf->layout.topLine->linenumber);
    if (n) {
      buf->layout.topLine = buf->layout.lineSkip(buf->layout.topLine, n, false);
      if (cur->real_linenumber > 0) {
        buf->layout.gotoRealLine(cur->real_linenumber);
      } else {
        buf->layout.gotoLine(cur->linenumber);
      }
    }
    buf->layout.pos -= buf->layout.currentLine->bpos;
    if (FoldLine && !buf->res->is_html_type()) {
      buf->layout.currentColumn = 0;
    } else {
      buf->layout.currentColumn = sbuf->layout.currentColumn;
    }
    buf->layout.arrangeCursor();
  }
  if (buf->check_url) {
    chkURLBuffer(buf);
  }
  formResetBuffer(&buf->layout, sbuf->layout.formitem().get());
}

std::shared_ptr<Buffer> forwardBuffer(const std::shared_ptr<Buffer> &first,
                                      const std::shared_ptr<Buffer> &buf) {
  std::shared_ptr<Buffer> b;
  for (b = first; b != nullptr && b->backBuffer != buf; b = b->backBuffer)
    ;
  return b;
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

/* append links */
static void append_link_info(const std::shared_ptr<Buffer> &buf, Str *html,
                             LinkList *link) {
  LinkList *l;
  Url pu;
  const char *url;

  if (!link)
    return;

  Strcat_charp(html, "<hr width=50%><h1>Link information</h1><table>\n");
  for (l = link; l; l = l->next) {
    if (l->url) {
      pu = urlParse(l->url, buf->res->getBaseURL());
      url = html_quote(pu.to_Str().c_str());
    } else
      url = "(empty)";
    Strcat_m_charp(html, "<tr valign=top><td><a href=\"", url, "\">",
                   l->title ? html_quote(l->title) : "(empty)", "</a><td>",
                   nullptr);
    if (l->type == LINK_TYPE_REL)
      Strcat_charp(html, "[Rel]");
    else if (l->type == LINK_TYPE_REV)
      Strcat_charp(html, "[Rev]");
    if (!l->url)
      url = "(empty)";
    else
      url = html_quote(url_decode0(l->url));
    Strcat_m_charp(html, "<td>", url, nullptr);
    if (l->ctype)
      Strcat_m_charp(html, " (", html_quote(l->ctype), ")", nullptr);
    Strcat_charp(html, "\n");
  }
  Strcat_charp(html, "</table>\n");
}

/*
 * information of current page and link
 */
std::shared_ptr<Buffer> page_info_panel(const std::shared_ptr<Buffer> &buf) {
  Str *tmp = Strnew_size(1024);
  Url pu;
  TextListItem *ti;
  struct frameset *f_set = nullptr;
  int all;
  const char *p, *q;

  Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");

  if (buf) {

    all = buf->layout.allLine;
    if (all == 0 && buf->layout.lastLine)
      all = buf->layout.lastLine->linenumber;
    p = url_decode0(buf->res->currentURL.to_Str().c_str());
    Strcat_m_charp(
        tmp, "<table cellpadding=0>", "<tr valign=top><td nowrap>Title<td>",
        html_quote(buf->layout.title.c_str()),
        "<tr valign=top><td nowrap>Current URL<td>", html_quote(p),
        "<tr valign=top><td nowrap>Document Type<td>",
        buf->res->type.size() ? html_quote(buf->res->type.c_str()) : "unknown",
        "<tr valign=top><td nowrap>Last Modified<td>",
        html_quote(last_modified(buf)), nullptr);
    Strcat_m_charp(tmp, "<tr valign=top><td nowrap>Number of lines<td>",
                   Sprintf("%d", all)->ptr,
                   "<tr valign=top><td nowrap>Transferred bytes<td>",
                   Sprintf("%lu", (unsigned long)buf->res->trbyte)->ptr,
                   nullptr);

    auto a = buf->layout.retrieveCurrentAnchor();
    if (a) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      p = Strnew(pu.to_Str())->ptr;
      q = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = q;
      Strcat_m_charp(
          tmp, "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"",
          q, "\">", p, "</a>", nullptr);
    }
    a = buf->layout.retrieveCurrentImg();
    if (a != nullptr) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      p = Strnew(pu.to_Str())->ptr;
      q = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = q;
      Strcat_m_charp(
          tmp, "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
          q, "\">", p, "</a>", nullptr);
    }
    a = buf->layout.retrieveCurrentForm();
    if (a != nullptr) {
      FormItemList *fi = (FormItemList *)a->url;
      p = form2str(fi);
      p = html_quote(url_decode0(p));
      Strcat_m_charp(
          tmp,
          "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>", p,
          nullptr);
      // if (fi->parent->method == FORM_METHOD_INTERNAL &&
      //     !Strcmp_charp(fi->parent->action, "map"))
      //   append_map_info(buf, tmp, fi->parent->item);
    }
    Strcat_charp(tmp, "</table>\n");

    append_link_info(buf, tmp, buf->layout.linklist);

    if (buf->res->document_header != nullptr) {
      Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
      for (ti = buf->res->document_header->first; ti != nullptr; ti = ti->next)
        Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr), "</pre_int>\n",
                       nullptr);
      Strcat_charp(tmp, "</pre>\n");
    }

    if (f_set) {
      Strcat_charp(tmp, "<hr width=50%><h1>Frame information</h1>\n");
      // append_frame_info(buf, tmp, f_set, 0);
    }
    if (buf->res->ssl_certificate)
      Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
                     html_quote(buf->res->ssl_certificate), "</pre>\n",
                     nullptr);
  }

  Strcat_charp(tmp, "</body></html>");
  auto newbuf = loadHTMLString(tmp);
  return newbuf;
}

#define DICTBUFFERNAME "*dictionary*"
void execdict(const char *word) {
  const char *w, *dictcmd;

  if (!UseDictCommand || word == nullptr || *word == '\0') {
    App::instance().invalidate();
    return;
  }
  w = word;
  if (*w == '\0') {
    App::instance().invalidate();
    return;
  }
  dictcmd =
      Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
  auto res = loadGeneralFile(dictcmd, {}, {.no_referer = true});
  if (!res) {
    disp_message("Execution failed");
    return;
  }

  auto buf = Buffer::create(res);
  // if (buf != NO_BUFFER)
  {
    buf->res->filename = w;
    buf->layout.title = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    if (buf->res->type.empty()) {
      buf->res->type = "text/plain";
    }
    CurrentTab->pushBuffer(buf);
  }
  App::instance().invalidate();
}

/* spawn external browser */
void invoke_browser(const char *url) {
  Str *cmd;
  int bg = 0, len;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto browser = App::instance().searchKeyData();
  if (browser == nullptr || *browser == '\0') {
    switch (prec_num) {
    case 0:
    case 1:
      browser = ExtBrowser;
      break;
    case 2:
      browser = ExtBrowser2;
      break;
    case 3:
      browser = ExtBrowser3;
      break;
    case 4:
      browser = ExtBrowser4;
      break;
    case 5:
      browser = ExtBrowser5;
      break;
    case 6:
      browser = ExtBrowser6;
      break;
    case 7:
      browser = ExtBrowser7;
      break;
    case 8:
      browser = ExtBrowser8;
      break;
    case 9:
      browser = ExtBrowser9;
      break;
    }
    if (browser == nullptr || *browser == '\0') {
      // browser = inputStr("Browse command: ", nullptr);
    }
  }
  if (browser == nullptr || *browser == '\0') {
    App::instance().invalidate();
    return;
  }

  if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' &&
      browser[len - 2] != '\\') {
    browser = allocStr(browser, len - 2);
    bg = 1;
  }
  cmd = myExtCommand((char *)browser, shell_quote(url), false);
  Strremovetrailingspaces(cmd);
  fmTerm();
  mySystem(cmd->ptr, bg);
  fmInit();
  App::instance().invalidate();
}

int checkBackBuffer(const std::shared_ptr<Buffer> &buf) {
  if (buf->backBuffer)
    return true;

  return false;
}

int cur_real_linenumber(const std::shared_ptr<Buffer> &buf) {
  Line *l, *cur = buf->layout.currentLine;
  int n;

  if (!cur)
    return 1;
  n = cur->real_linenumber ? cur->real_linenumber : 1;
  for (l = buf->layout.firstLine; l && l != cur && l->real_linenumber == 0;
       l = l->next) { /* header */
    if (l->bpos == 0)
      n++;
  }
  return n;
}

Str *Str_form_quote(Str *x) {
  Str *tmp = {};
  char *p = x->ptr, *ep = x->ptr + x->length;
  char buf[4];

  for (; p < ep; p++) {
    if (*p == ' ') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      Strcat_char(tmp, '+');
    } else if (is_url_unsafe(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)*p);
      Strcat_charp(tmp, buf);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp;
  return x;
}

static void _saveBuffer(const std::shared_ptr<Buffer> &buf, Line *l, FILE *f,
                        int cont) {

  auto is_html = buf->res->is_html_type();

  for (; l != nullptr; l = l->next) {
    Str *tmp;
    if (is_html)
      tmp = conv_symbol(l);
    else
      tmp = Strnew_charp_n(l->lineBuf.data(), l->len);
    Strfputs(tmp, f);
    if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
      putc('\n', f);
  }
}

void saveBuffer(const std::shared_ptr<Buffer> &buf, FILE *f, int cont) {
  _saveBuffer(buf, buf->layout.firstLine, f, cont);
}

void cmd_loadfile(const char *fn) {

  auto res = loadGeneralFile(file_to_url((char *)fn), {}, {.no_referer = true});
  if (!res) {
    char *emsg = Sprintf("%s not found", fn)->ptr;
    disp_err_message(emsg);
    return;
  }

  auto buf = Buffer::create(res);
  // if (buf != NO_BUFFER)
  { CurrentTab->pushBuffer(buf); }
  App::instance().invalidate();
}

std::shared_ptr<Buffer> link_list_panel(const std::shared_ptr<Buffer> &buf) {
  LinkList *l;
  Anchor *a;
  FormItemList *fi;
  const char *u, *p;
  const char *t;
  Url pu;
  Str *tmp = Strnew_charp("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

  if (buf->layout.linklist) {
    Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
    for (l = buf->layout.linklist; l; l = l->next) {
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

  if (buf->layout.href()) {
    Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
    auto al = buf->layout.href();
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->hseq < 0 || a->slave)
        continue;
      pu = urlParse(a->url, buf->res->getBaseURL());
      p = Strnew(pu.to_Str())->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      t = buf->layout.getAnchorText(al.get(), a);
      t = t ? html_quote(t) : "";
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  if (buf->layout.img()) {
    Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
    auto al = buf->layout.img();
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->slave)
        continue;
      pu = urlParse(a->url, buf->res->getBaseURL());
      p = Strnew(pu.to_Str())->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      if (a->title && *a->title)
        t = html_quote(a->title);
      else
        t = html_quote(url_decode0(a->url));
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
      if (!buf->layout.formitem()) {
        continue;
      }
      a = buf->layout.formitem()->retrieveAnchor(a->start.line, a->start.pos);
      if (!a)
        continue;
      fi = (FormItemList *)a->url;
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

  return loadHTMLString(tmp);
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(const std::shared_ptr<Buffer> &buf) {
  static const char *url_like_pat[] = {
      "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/"
      "=\\-]",
      "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
      "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
      "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
#ifdef INET6
      "https?://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./"
      "?=~_\\&+@#,\\$;]*",
      "ftp://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif /* INET6 */
      nullptr};
  int i;
  for (i = 0; url_like_pat[i]; i++) {
    buf->layout.reAnchor(url_like_pat[i]);
  }
  buf->check_url = true;
}

/* follow HREF link in the buffer */
void bufferA(void) {
  on_target = false;
  followA();
  on_target = true;
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
    buf->layout.title =
        Sprintf("source of %s", this->layout.title.c_str())->ptr;

    buf->res->currentURL = this->res->currentURL;
    buf->res->filename = this->res->filename;
    buf->res->sourcefile = this->res->sourcefile;
    buf->layout.need_reshape = true;

    return buf;
  } else if (CurrentTab->currentBuffer()->res->type == "text/plain") {
    auto buf = Buffer::create();
    buf->res->type = "text/html";
    buf->layout.title =
        Sprintf("HTML view of %s",
                CurrentTab->currentBuffer()->layout.title.c_str())
            ->ptr;

    buf->res->currentURL = this->res->currentURL;
    buf->res->filename = this->res->filename;
    buf->res->sourcefile = this->res->sourcefile;
    buf->layout.need_reshape = true;

    return buf;
  }

  return {};
}
