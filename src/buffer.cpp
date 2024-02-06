#include "buffer.h"
#include "history.h"
#include "quote.h"
#include "app.h"
#include "search.h"
#include "signal_util.h"
#include "search.h"
#include "tabbuffer.h"
#include "contentinfo.h"
#include "loadproc.h"
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

Buffer::Buffer(int width) : width(width) {
  this->info = std::make_shared<ContentInfo>();
  this->COLS = ::COLS;
  this->LINES = LASTLINE;
  this->clone = std::make_shared<Clone>();
  // use default from -o mark_all_pages
  this->check_url = MarkAllPages;
}

Buffer::~Buffer() {}

Buffer &Buffer::operator=(const Buffer &src) {
  this->info = src.info;
  this->buffername = src.buffername;
  this->firstLine = src.firstLine;
  this->topLine = src.topLine;
  this->currentLine = src.currentLine;
  this->lastLine = src.lastLine;
  this->nextBuffer = src.nextBuffer;
  this->linkBuffer = src.linkBuffer;
  this->width = src.width;
  this->height = src.height;
  this->allLine = src.allLine;
  this->bufferprop = src.bufferprop;
  this->currentColumn = src.currentColumn;
  this->cursorX = src.cursorX;
  this->cursorY = src.cursorY;
  this->pos = src.pos;
  this->visualpos = src.visualpos;
  this->rootX = src.rootX;
  this->rootY = src.rootY;
  this->COLS = src.COLS;
  this->LINES = src.LINES;
  this->href = src.href;
  this->name = src.name;
  this->img = src.img;
  this->formitem = src.formitem;
  this->linklist = src.linklist;
  this->formlist = src.formlist;
  this->maplist = src.maplist;
  this->hmarklist = src.hmarklist;
  this->imarklist = src.imarklist;
  this->baseTarget = src.baseTarget;
  this->real_schema = src.real_schema;
  this->sourcefile = src.sourcefile;
  this->clone = src.clone;
  this->trbyte = src.trbyte;
  this->check_url = src.check_url;
  this->form_submit = src.form_submit;
  this->edit = src.edit;
  this->mailcap = src.mailcap;
  this->mailcap_source = src.mailcap_source;
  this->ssl_certificate = src.ssl_certificate;
  this->image_flag = src.image_flag;
  this->image_loaded = src.image_loaded;
  this->need_reshape = src.need_reshape;
  this->submit = src.submit;
  this->undo = src.undo;
  this->event = src.event;
  return *this;
}

/*
 * Create null buffer
 */
Buffer *nullBuffer(void) {
  auto b = new Buffer(COLS);
  b->buffername = "*Null*";
  return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(Buffer *buf) {
  buf->firstLine = buf->topLine = buf->currentLine = buf->lastLine = nullptr;
  buf->allLine = 0;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(Buffer *buf) {
  int i;
  Buffer *b;

  clearBuffer(buf);
  for (i = 0; i < MAX_LB; i++) {
    b = buf->linkBuffer[i];
    if (b == nullptr)
      continue;
    b->linkBuffer[i] = nullptr;
  }
  if (--buf->clone->count)
    return;
  if (buf->sourcefile.size() &&
      (!buf->info->real_type ||
       strncasecmp(buf->info->real_type, "image/", 6))) {
  }
  if (buf->mailcap_source)
    unlink(buf->mailcap_source);
}

/*
 * namedBuffer: Select buffer which have specified name
 */
Buffer *namedBuffer(Buffer *first, char *name) {
  Buffer *buf;

  if (!strcmp(first->buffername, name)) {
    return first;
  }
  for (buf = first; buf->nextBuffer != nullptr; buf = buf->nextBuffer) {
    if (!strcmp(buf->nextBuffer->buffername, name)) {
      return buf->nextBuffer;
    }
  }
  return nullptr;
}

/*
 * replaceBuffer: replace buffer
 */
Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf) {
  Buffer *buf;

  if (delbuf == nullptr) {
    newbuf->nextBuffer = first;
    return newbuf;
  }
  if (first == delbuf) {
    newbuf->nextBuffer = delbuf->nextBuffer;
    discardBuffer(delbuf);
    return newbuf;
  }
  if (delbuf && (buf = prevBuffer(first, delbuf))) {
    buf->nextBuffer = newbuf;
    newbuf->nextBuffer = delbuf->nextBuffer;
    discardBuffer(delbuf);
    return first;
  }
  newbuf->nextBuffer = first;
  return newbuf;
}

Buffer *nthBuffer(Buffer *firstbuf, int n) {
  int i;
  Buffer *buf = firstbuf;

  if (n < 0)
    return firstbuf;
  for (i = 0; i < n; i++) {
    if (buf == nullptr)
      return nullptr;
    buf = buf->nextBuffer;
  }
  return buf;
}

static void writeBufferName(Buffer *buf, int n) {
  Str *msg;
  int all;

  all = buf->allLine;
  if (all == 0 && buf->lastLine != nullptr)
    all = buf->lastLine->linenumber;
  move(n, 0);
  msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
  if (buf->info->filename != nullptr) {
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

/*
 * gotoLine: go to line number
 */
void gotoLine(Buffer *buf, int n) {
  char msg[36];
  Line *l = buf->firstLine;

  if (l == nullptr)
    return;

  if (l->linenumber > n) {
    sprintf(msg, "First line is #%ld", l->linenumber);
    set_delayed_message(msg);
    buf->topLine = buf->currentLine = l;
    return;
  }
  if (buf->lastLine->linenumber < n) {
    l = buf->lastLine;
    sprintf(msg, "Last line is #%ld", buf->lastLine->linenumber);
    set_delayed_message(msg);
    buf->currentLine = l;
    buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1), false);
    return;
  }
  for (; l != nullptr; l = l->next) {
    if (l->linenumber >= n) {
      buf->currentLine = l;
      if (n < buf->topLine->linenumber ||
          buf->topLine->linenumber + buf->LINES <= n)
        buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, false);
      break;
    }
  }
}

/*
 * gotoRealLine: go to real line number
 */
void gotoRealLine(Buffer *buf, int n) {
  char msg[36];
  Line *l = buf->firstLine;

  if (l == nullptr)
    return;

  if (l->real_linenumber > n) {
    sprintf(msg, "First line is #%ld", l->real_linenumber);
    set_delayed_message(msg);
    buf->topLine = buf->currentLine = l;
    return;
  }
  if (buf->lastLine->real_linenumber < n) {
    l = buf->lastLine;
    sprintf(msg, "Last line is #%ld", buf->lastLine->real_linenumber);
    set_delayed_message(msg);
    buf->currentLine = l;
    buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1), false);
    return;
  }
  for (; l != nullptr; l = l->next) {
    if (l->real_linenumber >= n) {
      buf->currentLine = l;
      if (n < buf->topLine->real_linenumber ||
          buf->topLine->real_linenumber + buf->LINES <= n)
        buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, false);
      break;
    }
  }
}

static Buffer *listBuffer(Buffer *top, Buffer *current) {
  int i, c = 0;
  Buffer *buf = top;

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
    if (buf->nextBuffer == nullptr) {
      move(i + 1, 0);
      clrtobotx();
      break;
    }
    buf = buf->nextBuffer;
  }
  standout();
  /* FIXME: gettextize? */
  message("Buffer selection mode: SPC for select / D for delete buffer", 0, 0);
  standend();
  /*
   * move(LASTLINE, COLS - 1); */
  move(c, 0);
  refresh(term_io());
  return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
Buffer *selectBuffer(Buffer *firstbuf, Buffer *currentbuf, char *selectchar) {
  int i, cpoint,                  /* Current Buffer Number */
      spoint,                     /* Current Line on Screen */
      maxbuf, sclimit = LASTLINE; /* Upper limit of line * number in
                                   * the * screen */
  Buffer *buf, *topbuf;
  char c;

  i = cpoint = 0;
  for (buf = firstbuf; buf != nullptr; buf = buf->nextBuffer) {
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
        if (currentbuf->nextBuffer == nullptr)
          continue;
        writeBufferName(currentbuf, spoint);
        currentbuf = currentbuf->nextBuffer;
        cpoint++;
        spoint++;
        standout();
        writeBufferName(currentbuf, spoint);
        standend();
        move(spoint, 0);
        toggle_stand();
      } else if (cpoint < maxbuf - 1) {
        topbuf = currentbuf;
        currentbuf = currentbuf->nextBuffer;
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
void reshapeBuffer(Buffer *buf) {
  if (!buf->need_reshape) {
    return;
  }
  buf->need_reshape = false;

  buf->width = INIT_BUFFER_WIDTH();
  if (buf->sourcefile.empty()) {
    return;
  }

  UrlStream f(SCM_LOCAL);
  f.openFile(buf->mailcap_source ? buf->mailcap_source
                                 : buf->sourcefile.c_str());
  if (f.stream == nullptr) {
    return;
  }

  auto sbuf = new Buffer(0);
  *sbuf = *buf;
  clearBuffer(buf);

  buf->href = nullptr;
  buf->name = nullptr;
  buf->img = nullptr;
  buf->formitem = nullptr;
  buf->formlist = nullptr;
  buf->linklist = nullptr;
  buf->maplist = nullptr;
  if (buf->hmarklist)
    buf->hmarklist->nmark = 0;
  if (buf->imarklist)
    buf->imarklist->nmark = 0;

  if (is_html_type(buf->info->type))
    loadHTMLBuffer(&f, buf);
  else
    loadBuffer(&f, buf);
  f.close();

  buf->height = LASTLINE + 1;
  if (buf->firstLine && sbuf->firstLine) {
    Line *cur = sbuf->currentLine;
    int n;

    buf->pos = sbuf->pos + cur->bpos;
    while (cur->bpos && cur->prev)
      cur = cur->prev;
    if (cur->real_linenumber > 0)
      gotoRealLine(buf, cur->real_linenumber);
    else
      gotoLine(buf, cur->linenumber);
    n = (buf->currentLine->linenumber - buf->topLine->linenumber) -
        (cur->linenumber - sbuf->topLine->linenumber);
    if (n) {
      buf->topLine = lineSkip(buf, buf->topLine, n, false);
      if (cur->real_linenumber > 0)
        gotoRealLine(buf, cur->real_linenumber);
      else
        gotoLine(buf, cur->linenumber);
    }
    buf->pos -= buf->currentLine->bpos;
    if (FoldLine && !is_html_type(buf->info->type))
      buf->currentColumn = 0;
    else
      buf->currentColumn = sbuf->currentColumn;
    arrangeCursor(buf);
  }
  if (buf->check_url) {
    chkURLBuffer(buf);
  }
  formResetBuffer(buf, sbuf->formitem);
}

Buffer *prevBuffer(Buffer *first, Buffer *buf) {
  Buffer *b;

  for (b = first; b != nullptr && b->nextBuffer != buf; b = b->nextBuffer)
    ;
  return b;
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

/* append links */
static void append_link_info(Buffer *buf, Str *html, LinkList *link) {
  LinkList *l;
  Url pu;
  const char *url;

  if (!link)
    return;

  Strcat_charp(html, "<hr width=50%><h1>Link information</h1><table>\n");
  for (l = link; l; l = l->next) {
    if (l->url) {
      pu = urlParse(l->url, baseURL(buf));
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
Buffer *page_info_panel(Buffer *buf) {
  Str *tmp = Strnew_size(1024);
  Anchor *a;
  Url pu;
  TextListItem *ti;
  struct frameset *f_set = nullptr;
  int all;
  const char *p, *q;
  Buffer *newbuf;

  Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");

  if (buf == nullptr) {

    all = buf->allLine;
    if (all == 0 && buf->lastLine)
      all = buf->lastLine->linenumber;
    p = url_decode0(buf->info->currentURL.to_Str().c_str());
    Strcat_m_charp(
        tmp, "<table cellpadding=0>", "<tr valign=top><td nowrap>Title<td>",
        html_quote(buf->buffername),
        "<tr valign=top><td nowrap>Current URL<td>", html_quote(p),
        "<tr valign=top><td nowrap>Document Type<td>",
        buf->info->real_type ? html_quote(buf->info->real_type) : "unknown",
        "<tr valign=top><td nowrap>Last Modified<td>",
        html_quote(last_modified(buf)), nullptr);
    Strcat_m_charp(tmp, "<tr valign=top><td nowrap>Number of lines<td>",
                   Sprintf("%d", all)->ptr,
                   "<tr valign=top><td nowrap>Transferred bytes<td>",
                   Sprintf("%lu", (unsigned long)buf->trbyte)->ptr, nullptr);

    a = retrieveCurrentAnchor(buf);
    if (a) {
      pu = urlParse(a->url, baseURL(buf));
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
    a = retrieveCurrentImg(buf);
    if (a != nullptr) {
      pu = urlParse(a->url, baseURL(buf));
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
    a = retrieveCurrentForm(buf);
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

    append_link_info(buf, tmp, buf->linklist);

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
    if (buf->ssl_certificate)
      Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
                     html_quote(buf->ssl_certificate), "</pre>\n", nullptr);
  }

  Strcat_charp(tmp, "</body></html>");
  newbuf = loadHTMLString(tmp);
  return newbuf;
}

void Buffer::addnewline(const char *line, Lineprop *prop, int byteLen,
                        int breakWidth, int realLinenum) {
  {
    auto l = new Line(++this->allLine, this->currentLine, line, prop, byteLen,
                      realLinenum);
    this->pushLine(l);
  }

  if (byteLen <= 0 || breakWidth <= 0) {
    return;
  }

  while (auto l = this->currentLine->breakLine(breakWidth)) {
    this->pushLine(l);
  }
}

void set_buffer_environ(Buffer *buf) {
  static Buffer *prev_buf = nullptr;
  static Line *prev_line = nullptr;
  static int prev_pos = -1;
  Line *l;

  if (buf == nullptr)
    return;
  if (buf != prev_buf) {
    set_environ("W3M_SOURCEFILE", buf->sourcefile.c_str());
    set_environ("W3M_FILENAME", buf->info->filename);
    set_environ("W3M_TITLE", buf->buffername);
    set_environ("W3M_URL", buf->info->currentURL.to_Str().c_str());
    set_environ("W3M_TYPE",
                buf->info->real_type ? buf->info->real_type : "unknown");
  }
  l = buf->currentLine;
  if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos)) {
    Anchor *a;
    Url pu;
    char *s = GetWord(buf);
    set_environ("W3M_CURRENT_WORD", s ? s : "");
    a = retrieveCurrentAnchor(buf);
    if (a) {
      pu = urlParse(a->url, baseURL(buf));
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = retrieveCurrentImg(buf);
    if (a) {
      pu = urlParse(a->url, baseURL(buf));
      set_environ("W3M_CURRENT_IMG", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_IMG", "");
    a = retrieveCurrentForm(buf);
    if (a)
      set_environ("W3M_CURRENT_FORM", form2str((FormItemList *)a->url));
    else
      set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
    set_environ("W3M_CURRENT_COLUMN",
                Sprintf("%d", buf->currentColumn + buf->cursorX + 1)->ptr);
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
  prev_pos = buf->pos;
}

char *GetWord(Buffer *buf) {
  int b, e;
  char *p;
  if ((p = getCurWord(buf, &b, &e)) != nullptr) {
    return Strnew_charp_n(p, e - b)->ptr;
  }
  return nullptr;
}

char *getCurWord(Buffer *buf, int *spos, int *epos) {
  Line *l = buf->currentLine;
  int b;

  *spos = 0;
  *epos = 0;
  if (l == nullptr)
    return nullptr;
  auto p = l->lineBuf;
  auto e = buf->pos;
  while (e > 0 && !is_wordchar(getChar(&p[e])))
    prevChar(e, l);
  if (!is_wordchar(getChar(&p[e])))
    return nullptr;
  b = e;
  while (b > 0) {
    int tmp = b;
    prevChar(tmp, l);
    if (!is_wordchar(getChar(&p[tmp])))
      break;
    b = tmp;
  }
  while (e < l->len && is_wordchar(getChar(&p[e])))
    nextChar(e, l);
  *spos = b;
  *epos = e;
  return &p[b];
}

void shiftvisualpos(Buffer *buf, int shift) {
  Line *l = buf->currentLine;
  buf->visualpos -= shift;
  if (buf->visualpos - l->bwidth >= buf->COLS)
    buf->visualpos = l->bwidth + buf->COLS - 1;
  else if (buf->visualpos - l->bwidth < 0)
    buf->visualpos = l->bwidth;
  arrangeLine(buf);
  if (buf->visualpos - l->bwidth == -shift && buf->cursorX == 0)
    buf->visualpos = l->bwidth;
}

#define DICTBUFFERNAME "*dictionary*"
void execdict(const char *word) {
  const char *w, *dictcmd;
  Buffer *buf;

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
  buf = loadGeneralFile(dictcmd, {}, {.referer = NO_REFERER});
  if (buf == nullptr) {
    disp_message("Execution failed", true);
    return;
  } else if (buf != NO_BUFFER) {
    buf->info->filename = w;
    buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    if (buf->info->type == nullptr)
      buf->info->type = "text/plain";
    pushBuffer(buf);
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

  if (Currentbuf->firstLine == nullptr)
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
  a = (only_img ? nullptr : retrieveCurrentAnchor(Currentbuf));
  if (a == nullptr) {
    a = (only_img ? nullptr : retrieveCurrentForm(Currentbuf));
    if (a == nullptr) {
      a = retrieveCurrentImg(Currentbuf);
      if (a == nullptr)
        return;
    } else
      s = Strnew_charp(form2str((FormItemList *)a->url));
  }
  if (s == nullptr) {
    pu = urlParse(a->url, baseURL(Currentbuf));
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
int checkBackBuffer(Buffer *buf) {
  if (buf->nextBuffer)
    return true;

  return false;
}

/* go to the next downward/upward anchor */
void nextY(int d) {
  HmarkerList *hl = Currentbuf->hmarklist;
  Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  int hseq;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  x = Currentbuf->pos;
  y = Currentbuf->currentLine->linenumber + d;
  pan = nullptr;
  hseq = -1;
  for (i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = nullptr;
    for (; y >= 0 && y <= Currentbuf->lastLine->linenumber; y += d) {
      if (Currentbuf->href) {
        an = Currentbuf->href->retrieveAnchor(y, x);
      }
      if (!an && Currentbuf->formitem) {
        an = Currentbuf->formitem->retrieveAnchor(y, x);
      }
      if (an && hseq != abs(an->hseq)) {
        pan = an;
        break;
      }
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  gotoLine(Currentbuf, pan->start.line);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next left/right anchor */
void nextX(int d, int dy) {
  HmarkerList *hl = Currentbuf->hmarklist;
  Anchor *an, *pan;
  Line *l;
  int i, x, y, n = searchKeyNum();

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  l = Currentbuf->currentLine;
  x = Currentbuf->pos;
  y = l->linenumber;
  pan = nullptr;
  for (i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = nullptr;
    while (1) {
      for (; x >= 0 && x < l->len; x += d) {
        if (Currentbuf->href) {
          an = Currentbuf->href->retrieveAnchor(y, x);
        }
        if (!an && Currentbuf->formitem) {
          an = Currentbuf->formitem->retrieveAnchor(y, x);
        }
        if (an) {
          pan = an;
          break;
        }
      }
      if (!dy || an)
        break;
      l = (dy > 0) ? l->next : l->prev;
      if (!l)
        break;
      x = (d > 0) ? 0 : l->len - 1;
      y = l->linenumber;
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  gotoLine(Currentbuf, y);
  Currentbuf->pos = pan->start.pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the previous anchor */
void _prevA(int visited) {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  Url url;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (visited != true && an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  y = Currentbuf->currentLine->linenumber;
  x = Currentbuf->pos;

  if (visited == true) {
    n = hl->nmark;
  }

  for (i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq - 1;
      do {
        if (hseq < 0) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        po = hl->marks + hseq;
        if (Currentbuf->href) {
          an = Currentbuf->href->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && Currentbuf->formitem) {
          an = Currentbuf->formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq--;
        if (visited == true && an) {
          url = urlParse(an->url, baseURL(Currentbuf));
          if (getHashHist(URLHist, url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = closest_prev_anchor(Currentbuf->href, nullptr, x, y);
      if (visited != true)
        an = closest_prev_anchor(Currentbuf->formitem, an, x, y);
      if (an == nullptr) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true && an) {
        url = urlParse(an->url, baseURL(Currentbuf));
        if (getHashHist(URLHist, url.to_Str().c_str())) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == nullptr || an->hseq < 0)
    return;
  po = hl->marks + an->hseq;
  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next [visited] anchor */
void _nextA(int visited) {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  Url url;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (visited != true && an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  y = Currentbuf->currentLine->linenumber;
  x = Currentbuf->pos;

  if (visited == true) {
    n = hl->nmark;
  }

  for (i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq + 1;
      do {
        if (hseq >= hl->nmark) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        po = &hl->marks[hseq];
        if (Currentbuf->href) {
          an = Currentbuf->href->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && Currentbuf->formitem) {
          an = Currentbuf->formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq++;
        if (visited == true && an) {
          url = urlParse(an->url, baseURL(Currentbuf));
          if (getHashHist(URLHist, url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = closest_next_anchor(Currentbuf->href, nullptr, x, y);
      if (visited != true)
        an = closest_next_anchor(Currentbuf->formitem, an, x, y);
      if (an == nullptr) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true) {
        url = urlParse(an->url, baseURL(Currentbuf));
        if (getHashHist(URLHist, url.to_Str().c_str())) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == nullptr || an->hseq < 0)
    return;
  po = &hl->marks[an->hseq];
  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

int cur_real_linenumber(Buffer *buf) {
  Line *l, *cur = buf->currentLine;
  int n;

  if (!cur)
    return 1;
  n = cur->real_linenumber ? cur->real_linenumber : 1;
  for (l = buf->firstLine; l && l != cur && l->real_linenumber == 0;
       l = l->next) { /* header */
    if (l->bpos == 0)
      n++;
  }
  return n;
}

/* Move cursor left */
void _movL(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorLeft(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor downward */
void _movD(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorDown(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* move cursor upward */
void _movU(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorUp(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor right */
void _movR(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorRight(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */

int prev_nonnull_line(Line *line) {
  Line *l;

  for (l = line; l != nullptr && l->len == 0; l = l->prev)
    ;
  if (l == nullptr || l->len == 0)
    return -1;

  Currentbuf->currentLine = l;
  if (l != line)
    Currentbuf->pos = Currentbuf->currentLine->len;
  return 0;
}

int next_nonnull_line(Line *line) {
  Line *l;

  for (l = line; l != nullptr && l->len == 0; l = l->next)
    ;

  if (l == nullptr || l->len == 0)
    return -1;

  Currentbuf->currentLine = l;
  if (l != line)
    Currentbuf->pos = 0;
  return 0;
}

void repBuffer(Buffer *oldbuf, Buffer *buf) {
  Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
  Currentbuf = buf;
}

/* Go to specified line */
void _goLine(const char *l) {
  if (l == nullptr || *l == '\0' || Currentbuf->currentLine == nullptr) {
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  Currentbuf->pos = 0;
  if (((*l == '^') || (*l == '$')) && prec_num) {
    gotoRealLine(Currentbuf, prec_num);
  } else if (*l == '^') {
    Currentbuf->topLine = Currentbuf->currentLine = Currentbuf->firstLine;
  } else if (*l == '$') {
    Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->lastLine,
                                   -(Currentbuf->LINES + 1) / 2, true);
    Currentbuf->currentLine = Currentbuf->lastLine;
  } else
    gotoRealLine(Currentbuf, atoi(l));
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
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

std::optional<Url> baseURL(Buffer *buf) {
  if (buf->bufferprop & BP_NO_URL) {
    /* no URL is defined for the buffer */
    return {};
  }
  if (buf->info->baseURL) {
    /* <BASE> tag is defined in the document */
    return *buf->info->baseURL;
  } else if (buf->info->currentURL.IS_EMPTY_PARSED_URL()) {
    return {};
  } else {
    return buf->info->currentURL;
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
      sprintf(buf, "%%%02X", (unsigned char)*p);
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
