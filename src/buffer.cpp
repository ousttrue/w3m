#include "buffer.h"
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
#include "indep.h"
#include "istream.h"
#include "proto.h"
#include <unistd.h>

TabBuffer *CurrentTab;
TabBuffer *FirstTab;
TabBuffer *LastTab;
bool open_tab_blank = false;
bool open_tab_dl_list = false;
bool close_tab_back = false;
int nTab;
int TabCols = 10;

int REV_LB[MAX_LB] = {
    LB_N_FRAME, LB_FRAME, LB_N_INFO, LB_INFO, LB_N_SOURCE,
};

Buffer::Buffer(int width) : width(width) {
  this->COLS = ::COLS;
  this->LINES = LASTLINE;
  this->clone = std::make_shared<Clone>();
  this->check_url = MarkAllPages; /* use default from -o mark_all_pages */
}

Buffer::~Buffer() {}

Buffer &Buffer::operator=(const Buffer &src) {
  this->filename = src.filename;
  this->buffername = src.buffername;
  this->firstLine = src.firstLine;
  this->topLine = src.topLine;
  this->currentLine = src.currentLine;
  this->lastLine = src.lastLine;
  this->nextBuffer = src.nextBuffer;
  this->linkBuffer = src.linkBuffer;
  this->width = src.width;
  this->height = src.height;
  this->type = src.type;
  this->real_type = src.real_type;
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
  this->currentURL = src.currentURL;
  this->baseURL = src.baseURL;
  this->baseTarget = src.baseTarget;
  this->real_schema = src.real_schema;
  this->sourcefile = src.sourcefile;
  this->clone = src.clone;
  this->trbyte = src.trbyte;
  this->check_url = src.check_url;
  this->document_header = src.document_header;
  this->form_submit = src.form_submit;
  this->edit = src.edit;
  this->mailcap = src.mailcap;
  this->mailcap_source = src.mailcap_source;
  this->header_source = src.header_source;
  this->search_header = src.search_header;
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
  buf->firstLine = buf->topLine = buf->currentLine = buf->lastLine = NULL;
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
    if (b == NULL)
      continue;
    b->linkBuffer[REV_LB[i]] = NULL;
  }
  if (--buf->clone->count)
    return;
  if (buf->sourcefile &&
      (!buf->real_type || strncasecmp(buf->real_type, "image/", 6))) {
  }
  if (buf->header_source)
    unlink(buf->header_source);
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
  for (buf = first; buf->nextBuffer != NULL; buf = buf->nextBuffer) {
    if (!strcmp(buf->nextBuffer->buffername, name)) {
      return buf->nextBuffer;
    }
  }
  return NULL;
}

/*
 * deleteBuffer: delete buffer
 */
Buffer *deleteBuffer(Buffer *first, Buffer *delbuf) {
  Buffer *buf, *b;

  if (first == delbuf && first->nextBuffer != NULL) {
    buf = first->nextBuffer;
    discardBuffer(first);
    return buf;
  }
  if ((buf = prevBuffer(first, delbuf)) != NULL) {
    b = buf->nextBuffer;
    buf->nextBuffer = b->nextBuffer;
    discardBuffer(b);
  }
  return first;
}

/*
 * replaceBuffer: replace buffer
 */
Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf) {
  Buffer *buf;

  if (delbuf == NULL) {
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
    if (buf == NULL)
      return NULL;
    buf = buf->nextBuffer;
  }
  return buf;
}

static void writeBufferName(Buffer *buf, int n) {
  Str *msg;
  int all;

  all = buf->allLine;
  if (all == 0 && buf->lastLine != NULL)
    all = buf->lastLine->linenumber;
  move(n, 0);
  /* FIXME: gettextize? */
  msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
  if (buf->filename != NULL) {
    switch (buf->currentURL.schema) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
      if (strcmp(buf->currentURL.file, "-")) {
        Strcat_char(msg, ' ');
        Strcat_charp(msg, buf->currentURL.real_file);
      }
      break;
    case SCM_UNKNOWN:
    case SCM_MISSING:
      break;
    default:
      Strcat_char(msg, ' ');
      Strcat(msg, buf->currentURL.to_Str());
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

  if (l == NULL)
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
    buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1), FALSE);
    return;
  }
  for (; l != NULL; l = l->next) {
    if (l->linenumber >= n) {
      buf->currentLine = l;
      if (n < buf->topLine->linenumber ||
          buf->topLine->linenumber + buf->LINES <= n)
        buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
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

  if (l == NULL)
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
    buf->topLine = lineSkip(buf, buf->currentLine, -(buf->LINES - 1), FALSE);
    return;
  }
  for (; l != NULL; l = l->next) {
    if (l->real_linenumber >= n) {
      buf->currentLine = l;
      if (n < buf->topLine->real_linenumber ||
          buf->topLine->real_linenumber + buf->LINES <= n)
        buf->topLine = lineSkip(buf, l, -(buf->LINES + 1) / 2, FALSE);
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
    if (buf->nextBuffer == NULL) {
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
  for (buf = firstbuf; buf != NULL; buf = buf->nextBuffer) {
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
        if (currentbuf->nextBuffer == NULL)
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
  UrlStream f;

  if (!buf->need_reshape)
    return;
  buf->need_reshape = FALSE;
  buf->width = INIT_BUFFER_WIDTH;
  if (buf->sourcefile == NULL)
    return;
  init_stream(&f, SCM_LOCAL, NULL);
  examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile, &f);
  if (f.stream == NULL)
    return;

  auto sbuf = new Buffer(0);
  *sbuf = *buf;
  clearBuffer(buf);

  buf->href = NULL;
  buf->name = NULL;
  buf->img = NULL;
  buf->formitem = NULL;
  buf->formlist = NULL;
  buf->linklist = NULL;
  buf->maplist = NULL;
  if (buf->hmarklist)
    buf->hmarklist->nmark = 0;
  if (buf->imarklist)
    buf->imarklist->nmark = 0;

  if (buf->header_source) {
    if (buf->currentURL.schema != SCM_LOCAL || buf->mailcap_source ||
        !strcmp(buf->currentURL.file, "-")) {
      UrlStream h;
      init_stream(&h, SCM_LOCAL, NULL);
      examineFile(buf->header_source, &h);
      if (h.stream) {
        readHeader(&h, buf, TRUE, NULL);
        UFclose(&h);
      }
    } else if (buf->search_header) /* -m option */
      readHeader(&f, buf, TRUE, NULL);
  }

  if (is_html_type(buf->type))
    loadHTMLBuffer(&f, buf);
  else
    loadBuffer(&f, buf);
  UFclose(&f);

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
      buf->topLine = lineSkip(buf, buf->topLine, n, FALSE);
      if (cur->real_linenumber > 0)
        gotoRealLine(buf, cur->real_linenumber);
      else
        gotoLine(buf, cur->linenumber);
    }
    buf->pos -= buf->currentLine->bpos;
    if (FoldLine && !is_html_type(buf->type))
      buf->currentColumn = 0;
    else
      buf->currentColumn = sbuf->currentColumn;
    arrangeCursor(buf);
  }
  if (buf->check_url & CHK_URL)
    chkURLBuffer(buf);
  formResetBuffer(buf, sbuf->formitem);
}

Buffer *prevBuffer(Buffer *first, Buffer *buf) {
  Buffer *b;

  for (b = first; b != NULL && b->nextBuffer != buf; b = b->nextBuffer)
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
      pu = Url::parse2(l->url, baseURL(buf));
      url = html_quote(pu.to_Str().c_str());
    } else
      url = "(empty)";
    Strcat_m_charp(html, "<tr valign=top><td><a href=\"", url, "\">",
                   l->title ? html_quote(l->title) : "(empty)", "</a><td>",
                   NULL);
    if (l->type == LINK_TYPE_REL)
      Strcat_charp(html, "[Rel]");
    else if (l->type == LINK_TYPE_REV)
      Strcat_charp(html, "[Rev]");
    if (!l->url)
      url = "(empty)";
    else
      url = html_quote(url_decode0(l->url));
    Strcat_m_charp(html, "<td>", url, NULL);
    if (l->ctype)
      Strcat_m_charp(html, " (", html_quote(l->ctype), ")", NULL);
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
  struct frameset *f_set = NULL;
  int all;
  const char *p, *q;
  Buffer *newbuf;

  Strcat_charp(tmp, "<html><head>\
<title>Information about current page</title>\
</head><body>\
<h1>Information about current page</h1>\n");

  if (buf == NULL) {

    all = buf->allLine;
    if (all == 0 && buf->lastLine)
      all = buf->lastLine->linenumber;
    p = url_decode0(buf->currentURL.to_Str().c_str());
    Strcat_m_charp(tmp, "<table cellpadding=0>",
                   "<tr valign=top><td nowrap>Title<td>",
                   html_quote(buf->buffername),
                   "<tr valign=top><td nowrap>Current URL<td>", html_quote(p),
                   "<tr valign=top><td nowrap>Document Type<td>",
                   buf->real_type ? html_quote(buf->real_type) : "unknown",
                   "<tr valign=top><td nowrap>Last Modified<td>",
                   html_quote(last_modified(buf)), NULL);
    Strcat_m_charp(tmp, "<tr valign=top><td nowrap>Number of lines<td>",
                   Sprintf("%d", all)->ptr,
                   "<tr valign=top><td nowrap>Transferred bytes<td>",
                   Sprintf("%lu", (unsigned long)buf->trbyte)->ptr, NULL);

    a = retrieveCurrentAnchor(buf);
    if (a) {
      pu = Url::parse2(a->url, baseURL(buf));
      p = Strnew(pu.to_Str())->ptr;
      q = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = q;
      Strcat_m_charp(
          tmp, "<tr valign=top><td nowrap>URL of current anchor<td><a href=\"",
          q, "\">", p, "</a>", NULL);
    }
    a = retrieveCurrentImg(buf);
    if (a != NULL) {
      pu = Url::parse2(a->url, baseURL(buf));
      p = Strnew(pu.to_Str())->ptr;
      q = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = q;
      Strcat_m_charp(
          tmp, "<tr valign=top><td nowrap>URL of current image<td><a href=\"",
          q, "\">", p, "</a>", NULL);
    }
    a = retrieveCurrentForm(buf);
    if (a != NULL) {
      FormItemList *fi = (FormItemList *)a->url;
      p = form2str(fi);
      p = html_quote(url_decode0(p));
      Strcat_m_charp(
          tmp,
          "<tr valign=top><td nowrap>Method/type of current form&nbsp;<td>", p,
          NULL);
      // if (fi->parent->method == FORM_METHOD_INTERNAL &&
      //     !Strcmp_charp(fi->parent->action, "map"))
      //   append_map_info(buf, tmp, fi->parent->item);
    }
    Strcat_charp(tmp, "</table>\n");

    append_link_info(buf, tmp, buf->linklist);

    if (buf->document_header != NULL) {
      Strcat_charp(tmp, "<hr width=50%><h1>Header information</h1><pre>\n");
      for (ti = buf->document_header->first; ti != NULL; ti = ti->next)
        Strcat_m_charp(tmp, "<pre_int>", html_quote(ti->ptr), "</pre_int>\n",
                       NULL);
      Strcat_charp(tmp, "</pre>\n");
    }

    if (f_set) {
      Strcat_charp(tmp, "<hr width=50%><h1>Frame information</h1>\n");
      // append_frame_info(buf, tmp, f_set, 0);
    }
    if (buf->ssl_certificate)
      Strcat_m_charp(tmp, "<h1>SSL certificate</h1><pre>\n",
                     html_quote(buf->ssl_certificate), "</pre>\n", NULL);
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
  static Buffer *prev_buf = NULL;
  static Line *prev_line = NULL;
  static int prev_pos = -1;
  Line *l;

  if (buf == NULL)
    return;
  if (buf != prev_buf) {
    set_environ("W3M_SOURCEFILE", buf->sourcefile);
    set_environ("W3M_FILENAME", buf->filename);
    set_environ("W3M_TITLE", buf->buffername);
    set_environ("W3M_URL", buf->currentURL.to_Str().c_str());
    set_environ("W3M_TYPE", buf->real_type ? buf->real_type : "unknown");
  }
  l = buf->currentLine;
  if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos)) {
    Anchor *a;
    Url pu;
    char *s = GetWord(buf);
    set_environ("W3M_CURRENT_WORD", s ? s : "");
    a = retrieveCurrentAnchor(buf);
    if (a) {
      pu = Url::parse2(a->url, baseURL(buf));
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = retrieveCurrentImg(buf);
    if (a) {
      pu = Url::parse2(a->url, baseURL(buf));
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
  if ((p = getCurWord(buf, &b, &e)) != NULL) {
    return Strnew_charp_n(p, e - b)->ptr;
  }
  return NULL;
}

char *getCurWord(Buffer *buf, int *spos, int *epos) {
  Line *l = buf->currentLine;
  int b;

  *spos = 0;
  *epos = 0;
  if (l == NULL)
    return NULL;
  auto p = l->lineBuf;
  auto e = buf->pos;
  while (e > 0 && !is_wordchar(getChar(&p[e])))
    prevChar(e, l);
  if (!is_wordchar(getChar(&p[e])))
    return NULL;
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
