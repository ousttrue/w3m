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
#include "readbuffer.h"
#include "rc.h"
#include "textlist.h"
#include "linklist.h"
#include "tmpfile.h"
#include "terms.h"
#include "form.h"
#include "anchor.h"
#include "ctrlcode.h"
#include "line.h"
#include "istream.h"
#include "proto.h"
#include <unistd.h>

Buffer::Buffer(int width) {
  this->layout.width = width;
  this->info = std::make_shared<HttpResponse>();
  this->clone = std::make_shared<Clone>();
  // use default from -o mark_all_pages
  this->check_url = MarkAllPages;
}

Buffer::~Buffer() {}

Buffer &Buffer::operator=(const Buffer &src) {
  this->info = src.info;
  this->layout = src.layout;
  this->backBuffer = src.backBuffer;
  this->linkBuffer = src.linkBuffer;
  this->clone = src.clone;
  this->check_url = src.check_url;
  return *this;
}

/*
 * Create null buffer
 */
std::shared_ptr<Buffer> nullBuffer(void) {
  auto b = Buffer::create(COLS);
  b->layout.title = "*Null*";
  return b;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(const std::shared_ptr<Buffer> &buf) {
  buf->layout.clearBuffer();
  for (int i = 0; i < MAX_LB; i++) {
    auto b = buf->linkBuffer[i];
    if (b == nullptr)
      continue;
    b->linkBuffer[i] = nullptr;
  }
  if (--buf->clone->count) {
    return;
  }
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
  if (buf->info->filename.size()) {
    switch (buf->info->currentURL.schema) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
      if (buf->info->currentURL.file != "-") {
        Strcat_char(msg, ' ');
        Strcat(msg, buf->info->currentURL.real_file);
      }
      break;
    case SCM_UNKNOWN:
    case SCM_MISSING:
      break;
    default:
      Strcat_char(msg, ' ');
      Strcat(msg, buf->info->currentURL.to_Str());
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
  if (buf->info->sourcefile.empty()) {
    return;
  }

  buf->info->f.openFile(buf->info->sourcefile.c_str());
  if (buf->info->f.stream == nullptr) {
    return;
  }

  auto sbuf = Buffer::create(0);
  *sbuf = *buf;
  buf->layout.clearBuffer();

  buf->layout.href = nullptr;
  buf->layout.name = nullptr;
  buf->layout.img = nullptr;
  buf->layout.formitem = nullptr;
  buf->layout.formlist = nullptr;
  buf->layout.linklist = nullptr;
  buf->layout.maplist = nullptr;
  if (buf->layout.hmarklist)
    buf->layout.hmarklist->nmark = 0;
  if (buf->layout.imarklist)
    buf->layout.imarklist->nmark = 0;

  if (buf->info->is_html_type())
    loadHTMLstream(buf->info.get(), &buf->layout);
  else
    loadBuffer(buf->info.get(), &buf->layout);

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
    if (FoldLine && !buf->info->is_html_type()) {
      buf->layout.currentColumn = 0;
    } else {
      buf->layout.currentColumn = sbuf->layout.currentColumn;
    }
    buf->layout.arrangeCursor();
  }
  if (buf->check_url) {
    chkURLBuffer(buf);
  }
  formResetBuffer(&buf->layout, sbuf->layout.formitem);
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
      pu = urlParse(l->url, buf->info->getBaseURL());
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
  Anchor *a;
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
    p = url_decode0(buf->info->currentURL.to_Str().c_str());
    Strcat_m_charp(tmp, "<table cellpadding=0>",
                   "<tr valign=top><td nowrap>Title<td>",
                   html_quote(buf->layout.title.c_str()),
                   "<tr valign=top><td nowrap>Current URL<td>", html_quote(p),
                   "<tr valign=top><td nowrap>Document Type<td>",
                   buf->info->type.size() ? html_quote(buf->info->type.c_str())
                                          : "unknown",
                   "<tr valign=top><td nowrap>Last Modified<td>",
                   html_quote(last_modified(buf)), nullptr);
    Strcat_m_charp(tmp, "<tr valign=top><td nowrap>Number of lines<td>",
                   Sprintf("%d", all)->ptr,
                   "<tr valign=top><td nowrap>Transferred bytes<td>",
                   Sprintf("%lu", (unsigned long)buf->info->trbyte)->ptr,
                   nullptr);

    a = retrieveCurrentAnchor(&buf->layout);
    if (a) {
      pu = urlParse(a->url, buf->info->getBaseURL());
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
    a = retrieveCurrentImg(&buf->layout);
    if (a != nullptr) {
      pu = urlParse(a->url, buf->info->getBaseURL());
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
    a = retrieveCurrentForm(&buf->layout);
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

    if (buf->info->document_header != nullptr) {
      Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
      for (ti = buf->info->document_header->first; ti != nullptr; ti = ti->next)
        Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr), "</pre_int>\n",
                       nullptr);
      Strcat_charp(tmp, "</pre>\n");
    }

    if (f_set) {
      Strcat_charp(tmp, "<hr width=50%><h1>Frame information</h1>\n");
      // append_frame_info(buf, tmp, f_set, 0);
    }
    if (buf->info->ssl_certificate)
      Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
                     html_quote(buf->info->ssl_certificate), "</pre>\n",
                     nullptr);
  }

  Strcat_charp(tmp, "</body></html>");
  auto newbuf = loadHTMLString(tmp);
  return newbuf;
}

void set_buffer_environ(const std::shared_ptr<Buffer> &buf) {
  static std::shared_ptr<Buffer> prev_buf;
  static Line *prev_line = nullptr;
  static int prev_pos = -1;
  Line *l;

  if (buf == nullptr)
    return;
  if (buf != prev_buf) {
    set_environ("W3M_SOURCEFILE", buf->info->sourcefile.c_str());
    set_environ("W3M_FILENAME", buf->info->filename.c_str());
    set_environ("W3M_TITLE", buf->layout.title.c_str());
    set_environ("W3M_URL", buf->info->currentURL.to_Str().c_str());
    set_environ("W3M_TYPE",
                buf->info->type.size() ? buf->info->type.c_str() : "unknown");
  }
  l = buf->layout.currentLine;
  if (l && (buf != prev_buf || l != prev_line || buf->layout.pos != prev_pos)) {
    Anchor *a;
    Url pu;
    char *s = GetWord(buf);
    set_environ("W3M_CURRENT_WORD", s ? s : "");
    a = retrieveCurrentAnchor(&buf->layout);
    if (a) {
      pu = urlParse(a->url, buf->info->getBaseURL());
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = retrieveCurrentImg(&buf->layout);
    if (a) {
      pu = urlParse(a->url, buf->info->getBaseURL());
      set_environ("W3M_CURRENT_IMG", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_IMG", "");
    a = retrieveCurrentForm(&buf->layout);
    if (a)
      set_environ("W3M_CURRENT_FORM", form2str((FormItemList *)a->url));
    else
      set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
    set_environ("W3M_CURRENT_COLUMN", Sprintf("%d", buf->layout.currentColumn +
                                                        buf->layout.cursorX + 1)
                                          ->ptr);
  } else if (!l) {
    set_environ("W3M_CURRENT_WORD", "");
    set_environ("W3M_CURRENT_LINK", "");
    set_environ("W3M_CURRENT_IMG", "");
    set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", "0");
    set_environ("W3M_CURRENT_COLUMN", "0");
  }
  prev_buf = buf;
  prev_line = l;
  prev_pos = buf->layout.pos;
}

char *GetWord(const std::shared_ptr<Buffer> &buf) {
  int b, e;
  char *p;
  if ((p = getCurWord(buf, &b, &e)) != nullptr) {
    return Strnew_charp_n(p, e - b)->ptr;
  }
  return nullptr;
}

char *getCurWord(const std::shared_ptr<Buffer> &buf, int *spos, int *epos) {
  Line *l = buf->layout.currentLine;
  int b;

  *spos = 0;
  *epos = 0;
  if (l == nullptr)
    return nullptr;
  auto &p = l->lineBuf;
  auto e = buf->layout.pos;
  while (e > 0 && !is_wordchar(p[e]))
    prevChar(e, l);
  if (!is_wordchar(p[e]))
    return nullptr;
  b = e;
  while (b > 0) {
    int tmp = b;
    prevChar(tmp, l);
    if (!is_wordchar(p[tmp]))
      break;
    b = tmp;
  }
  while (e < l->len && is_wordchar(p[e])) {
    e++;
  }
  *spos = b;
  *epos = e;
  return &p[b];
}

#define DICTBUFFERNAME "*dictionary*"
void execdict(const char *word) {
  const char *w, *dictcmd;

  if (!UseDictCommand || word == nullptr || *word == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  w = word;
  if (*w == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  dictcmd =
      Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
  auto res = loadGeneralFile(dictcmd, {}, {.referer = NO_REFERER});
  if (!res) {
    disp_message("Execution failed", true);
    return;
  }

  auto buf = Buffer::create(INIT_BUFFER_WIDTH());
  buf->info = res;
  // if (buf != NO_BUFFER)
  {
    buf->info->filename = w;
    buf->layout.title = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    if (buf->info->type.empty()) {
      buf->info->type = "text/plain";
    }
    CurrentTab->pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* spawn external browser */
void invoke_browser(const char *url) {
  Str *cmd;
  const char *browser = nullptr;
  int bg = 0, len;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  browser = searchKeyData();
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
    displayBuffer(Currentbuf, B_NORMAL);
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
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void _peekURL(int only_img) {

  Anchor *a;
  Url pu;
  static Str *s = nullptr;
  static int offset = 0, n;

  if (Currentbuf->layout.firstLine == nullptr)
    return;
  if (CurrentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
    goto disp;
  } else {
    offset = 0;
  }
  s = nullptr;
  a = (only_img ? nullptr : retrieveCurrentAnchor(&Currentbuf->layout));
  if (a == nullptr) {
    a = (only_img ? nullptr : retrieveCurrentForm(&Currentbuf->layout));
    if (a == nullptr) {
      a = retrieveCurrentImg(&Currentbuf->layout);
      if (a == nullptr)
        return;
    } else
      s = Strnew_charp(form2str((FormItemList *)a->url));
  }
  if (s == nullptr) {
    pu = urlParse(a->url, Currentbuf->info->getBaseURL());
    s = Strnew(pu.to_Str());
  }
  if (DecodeURL)
    s = Strnew_charp(url_decode0(s->ptr));
disp:
  n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  disp_message(&s->ptr[offset], true);
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

#define conv_form_encoding(val, fi, buf) (val)
void query_from_followform(Str **query, FormItemList *fi, int multipart) {
  FormItemList *f2;
  FILE *body = nullptr;

  if (multipart) {
    *query = tmpfname(TMPF_DFL, nullptr);
    body = fopen((*query)->ptr, "w");
    if (body == nullptr) {
      return;
    }
    fi->parent->body = (*query)->ptr;
    fi->parent->boundary =
        Sprintf("------------------------------%d%ld%ld%ld", CurrentPid,
                fi->parent, fi->parent->body, fi->parent->boundary)
            ->ptr;
  }
  *query = Strnew();
  for (f2 = fi->parent->item; f2; f2 = f2->next) {
    if (f2->name == nullptr)
      continue;
    /* <ISINDEX> is translated into single text form */
    if (f2->name->length == 0 && (multipart || f2->type != FORM_INPUT_TEXT))
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2 != fi || f2->value == nullptr)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;
    }
    if (multipart) {
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        *query = conv_form_encoding(f2->name, fi, Currentbuf)->Strdup();
        Strcat_charp(*query, ".x");
        form_write_data(body, fi->parent->boundary, (*query)->ptr,
                        Sprintf("%d", x)->ptr);
        *query = conv_form_encoding(f2->name, fi, Currentbuf)->Strdup();
        Strcat_charp(*query, ".y");
        form_write_data(body, fi->parent->boundary, (*query)->ptr,
                        Sprintf("%d", y)->ptr);
      } else if (f2->name && f2->name->length > 0 && f2->value != nullptr) {
        /* not IMAGE */
        *query = conv_form_encoding(f2->value, fi, Currentbuf);
        if (f2->type == FORM_INPUT_FILE)
          form_write_from_file(
              body, fi->parent->boundary,
              conv_form_encoding(f2->name, fi, Currentbuf)->ptr, (*query)->ptr,
              f2->value->ptr);
        else
          form_write_data(body, fi->parent->boundary,
                          conv_form_encoding(f2->name, fi, Currentbuf)->ptr,
                          (*query)->ptr);
      }
    } else {
      /* not multipart */
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        Strcat(*query,
               Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
        Strcat(*query, Sprintf(".x=%d&", x));
        Strcat(*query,
               Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
        Strcat(*query, Sprintf(".y=%d", y));
      } else {
        /* not IMAGE */
        if (f2->name && f2->name->length > 0) {
          Strcat(*query,
                 Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
          Strcat_char(*query, '=');
        }
        if (f2->value != nullptr) {
          if (fi->parent->method == FORM_METHOD_INTERNAL)
            Strcat(*query, Str_form_quote(f2->value));
          else {
            Strcat(*query, Str_form_quote(
                               conv_form_encoding(f2->value, fi, Currentbuf)));
          }
        }
      }
      if (f2->next)
        Strcat_char(*query, '&');
    }
  }
  if (multipart) {
    fprintf(body, "--%s--\r\n", fi->parent->boundary);
    fclose(body);
  } else {
    /* remove trailing & */
    while (Strlastchar(*query) == '&')
      Strshrink(*query, 1);
  }
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

  auto is_html = buf->info->is_html_type();

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

void cmd_loadBuffer(const std::shared_ptr<Buffer> &buf, int linkid) {
  if (buf == nullptr) {
    disp_err_message("Can't load string", false);
  } else /*if (buf != NO_BUFFER)*/ {
    buf->info->currentURL = Currentbuf->info->currentURL;
    if (linkid != LB_NOLINK) {
      buf->linkBuffer[linkid] = Currentbuf;
      Currentbuf->linkBuffer[linkid] = buf;
    }
    CurrentTab->pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void cmd_loadfile(const char *fn) {

  auto res =
      loadGeneralFile(file_to_url((char *)fn), {}, {.referer = NO_REFERER});
  if (!res) {
    char *emsg = Sprintf("%s not found", fn)->ptr;
    disp_err_message(emsg, false);
    return;
  }

  auto buf = Buffer::create(INIT_BUFFER_WIDTH());
  buf->info = res;
  // if (buf != NO_BUFFER)
  { CurrentTab->pushBuffer(buf); }
  displayBuffer(Currentbuf, B_NORMAL);
}

std::shared_ptr<Buffer> link_list_panel(const std::shared_ptr<Buffer> &buf) {
  LinkList *l;
  AnchorList *al;
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
        pu = Url::parse(l->url, buf->info->getBaseURL());
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

  if (buf->layout.href) {
    Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
    al = buf->layout.href;
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->hseq < 0 || a->slave)
        continue;
      pu = urlParse(a->url, buf->info->getBaseURL());
      p = Strnew(pu.to_Str())->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      t = getAnchorText(&buf->layout, al, a);
      t = t ? html_quote(t) : "";
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  if (buf->layout.img) {
    Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
    al = buf->layout.img;
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->slave)
        continue;
      pu = urlParse(a->url, buf->info->getBaseURL());
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
      if (!buf->layout.formitem) {
        continue;
      }
      a = buf->layout.formitem->retrieveAnchor(a->start.line, a->start.pos);
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
