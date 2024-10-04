#include "buffer.h"
#include "istream.h"
#include "map.h"
#include "document.h"
#include "file.h"
#include "indep.h"
#include "url_stream.h"
#include "strcase.h"
#include "http_response.h"
#include "alloc.h"
#include "file.h"
#include "app.h"
#include "html_readbuffer.h"
#include "url.h"
#include "ctrlcode.h"
#include "fm.h"
#include "tty.h"
#include "scr.h"
#include "termsize.h"
#include "terms.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

int FoldLine = false;
int showLineNum = false;

int REV_LB[MAX_LB] = {
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};

/*
 * Buffer creation
 */
struct Buffer *newBuffer() {
  struct Buffer *n = New(struct Buffer);
  memset((void *)n, 0, sizeof(struct Buffer));
  n->document = newDocument(INIT_BUFFER_WIDTH);
  n->currentURL.scheme = SCM_UNKNOWN;
  n->buffername = "";
  n->bufferprop = BP_NORMAL;
  n->clone = New(int);
  *n->clone = 1;
  n->trbyte = 0;
  n->ssl_certificate = NULL;
  n->check_url = MarkAllPages; /* use default from -o mark_all_pages */
  n->need_reshape = 1;         /* always reshape new buffers to mark URLs */
  return n;
}

/*
 * Create null buffer
 */
struct Buffer *nullBuffer(void) {
  auto b = newBuffer();
  b->buffername = "*Null*";
  return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(struct Buffer *buf) {
  buf->document->firstLine = buf->document->topLine =
      buf->document->currentLine = buf->document->lastLine = NULL;
  buf->document->allLine = 0;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(struct Buffer *buf) {
  clearBuffer(buf);
  for (int i = 0; i < MAX_LB; i++) {
    auto b = buf->linkBuffer[i];
    if (b == NULL)
      continue;
    b->linkBuffer[REV_LB[i]] = NULL;
  }

  if (buf->document->savecache)
    unlink(buf->document->savecache);
  if (--(*buf->clone))
    return;

  if (buf->sourcefile &&
      (!buf->real_type || strncasecmp(buf->real_type, "image/", 6))) {
    if (buf->real_scheme != SCM_LOCAL || buf->bufferprop & BP_FRAME)
      unlink(buf->sourcefile);
  }
}

/*
 * namedBuffer: Select buffer which have specified name
 */
struct Buffer *namedBuffer(struct Buffer *first, char *name) {
  struct Buffer *buf;

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
struct Buffer *deleteBuffer(struct Buffer *first, struct Buffer *delbuf) {
  struct Buffer *buf, *b;

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
struct Buffer *replaceBuffer(struct Buffer *first, struct Buffer *delbuf,
                             struct Buffer *newbuf) {
  struct Buffer *buf;

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

struct Buffer *nthBuffer(struct Buffer *firstbuf, int n) {
  int i;
  struct Buffer *buf = firstbuf;

  if (n < 0)
    return firstbuf;
  for (i = 0; i < n; i++) {
    if (buf == NULL)
      return NULL;
    buf = buf->nextBuffer;
  }
  return buf;
}

static void writeBufferName(struct Buffer *buf, int n) {
  Str msg;
  int all;

  all = buf->document->allLine;
  if (all == 0 && buf->document->lastLine != NULL)
    all = buf->document->lastLine->linenumber;
  scr_move(n, 0);
  /* FIXME: gettextize? */
  msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
  if (buf->filename != NULL) {
    switch (buf->currentURL.scheme) {
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
      Strcat(msg, parsedURL2Str(&buf->currentURL));
      break;
    }
  }
  scr_addnstr_sup(msg->ptr, COLS - 1);
}

/*
 * gotoRealLine: go to real line number
 */
void gotoRealLine(struct Buffer *buf, int n) {
  char msg[32];
  struct Line *l = buf->document->firstLine;

  if (l == NULL)
    return;

  if (l->real_linenumber > n) {
    /* FIXME: gettextize? */
    sprintf(msg, "First line is #%ld", l->real_linenumber);
    set_delayed_message(msg);
    buf->document->topLine = buf->document->currentLine = l;
    return;
  }
  if (buf->document->lastLine->real_linenumber < n) {
    l = buf->document->lastLine;
    /* FIXME: gettextize? */
    sprintf(msg, "Last line is #%ld", buf->document->lastLine->real_linenumber);
    set_delayed_message(msg);
    buf->document->currentLine = l;
    buf->document->topLine = lineSkip(buf->document, buf->document->currentLine,
                                      -(buf->document->LINES - 1), false);
    return;
  }
  for (; l != NULL; l = l->next) {
    if (l->real_linenumber >= n) {
      buf->document->currentLine = l;
      if (n < buf->document->topLine->real_linenumber ||
          buf->document->topLine->real_linenumber + buf->document->LINES <= n)
        buf->document->topLine =
            lineSkip(buf->document, l, -(buf->document->LINES + 1) / 2, false);
      break;
    }
  }
}

static struct Buffer *listBuffer(struct Buffer *top, struct Buffer *current) {
  int i, c = 0;
  struct Buffer *buf = top;

  scr_move(0, 0);
  scr_clrtobotx();
  for (i = 0; i < LASTLINE; i++) {
    if (buf == current) {
      c = i;
      scr_standout();
    }
    writeBufferName(buf, i);
    if (buf == current) {
      scr_standend();
      scr_clrtoeolx();
      scr_move(i, 0);
      scr_toggle_stand();
    } else
      scr_clrtoeolx();
    if (buf->nextBuffer == NULL) {
      scr_move(i + 1, 0);
      scr_clrtobotx();
      break;
    }
    buf = buf->nextBuffer;
  }
  scr_standout();
  /* FIXME: gettextize? */
  scr_message("Buffer selection mode: SPC for select / D for delete buffer", 0,
              0);
  scr_standend();
  /*
   * scr_move(LASTLINE, COLS - 1); */
  scr_move(c, 0);
  term_refresh();
  return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
struct Buffer *selectBuffer(struct Buffer *firstbuf, struct Buffer *currentbuf,
                            char *selectchar) {
  int i, cpoint,                  /* Current Buffer Number */
      spoint,                     /* Current Line on Screen */
      maxbuf, sclimit = LASTLINE; /* Upper limit of line * number in
                                   * the * screen */
  struct Buffer *buf, *topbuf;
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
    if ((c = tty_getch()) == ESC_CODE) {
      if ((c = tty_getch()) == '[' || c == 'O') {
        switch (c = tty_getch()) {
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
        scr_standout();
        writeBufferName(currentbuf, spoint);
        scr_standend();
        scr_move(spoint, 0);
        scr_toggle_stand();
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
        scr_standout();
        writeBufferName(currentbuf, spoint);
        scr_standend();
        scr_move(spoint, 0);
        scr_toggle_stand();
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
     * scr_move(LASTLINE, COLS - 1);
     */
    scr_move(spoint, 0);
    term_refresh();
  }
}

/*
 * Reshape HTML buffer
 */
void reshapeBuffer(struct Buffer *buf) {
  if (!buf->need_reshape)
    return;
  buf->need_reshape = false;

  buf->document->width = INIT_BUFFER_WIDTH;
  if (buf->sourcefile == NULL)
    return;

  struct URLFile f;
  init_stream(&f, SCM_LOCAL, NULL);
  examineFile(buf->sourcefile, &f);
  if (f.stream == NULL)
    return;

  struct Document sbuf;
  copyBuffer(&sbuf, buf->document);
  clearBuffer(buf);

  buf->document->href = NULL;
  buf->document->name = NULL;
  buf->document->img = NULL;
  buf->document->formitem = NULL;
  buf->document->formlist = NULL;
  buf->document->linklist = NULL;
  buf->document->maplist = NULL;
  if (buf->document->hmarklist)
    buf->document->hmarklist->nmark = 0;
  if (buf->document->imarklist)
    buf->document->imarklist->nmark = 0;

  if (is_html_type(buf->type))
    loadHTMLBuffer(&f, nullptr, buf);
  else
    loadBuffer(&f, nullptr, buf);
  UFclose(&f);

  buf->document->height = LASTLINE + 1;
  if (buf->document->firstLine && sbuf.firstLine) {
    struct Line *cur = sbuf.currentLine;
    int n;

    buf->document->pos = sbuf.pos + cur->bpos;
    while (cur->bpos && cur->prev)
      cur = cur->prev;
    if (cur->real_linenumber > 0)
      gotoRealLine(buf, cur->real_linenumber);
    else
      gotoLine(buf->document, cur->linenumber);
    n = (buf->document->currentLine->linenumber -
         buf->document->topLine->linenumber) -
        (cur->linenumber - sbuf.topLine->linenumber);
    if (n) {
      buf->document->topLine =
          lineSkip(buf->document, buf->document->topLine, n, false);
      if (cur->real_linenumber > 0)
        gotoRealLine(buf, cur->real_linenumber);
      else
        gotoLine(buf->document, cur->linenumber);
    }
    buf->document->pos -= buf->document->currentLine->bpos;
    if (FoldLine && !is_html_type(buf->type))
      buf->document->currentColumn = 0;
    else
      buf->document->currentColumn = sbuf.currentColumn;
    arrangeCursor(buf->document);
  }
  if (buf->check_url & CHK_URL)
    chkURLBuffer(buf);
  formResetBuffer(buf, sbuf.formitem);
}

struct Buffer *prevBuffer(struct Buffer *first, struct Buffer *buf) {
  struct Buffer *b;

  for (b = first; b != NULL && b->nextBuffer != buf; b = b->nextBuffer)
    ;
  return b;
}

/* get last modified time */
char *last_modified(struct Buffer *buf) {
  struct TextListItem *ti;
  struct stat st;

  if (buf->http_response->document_header) {
    for (ti = buf->http_response->document_header->first; ti; ti = ti->next) {
      if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0) {
        return ti->ptr + 15;
      }
    }
    return "unknown";
  } else if (buf->currentURL.scheme == SCM_LOCAL) {
    if (stat(buf->currentURL.file, &st) < 0)
      return "unknown";
    return ctime(&st.st_mtime);
  }
  return "unknown";
}

const char *guess_save_name(struct Buffer *buf, const char *path) {
  if (buf && buf->http_response->document_header) {
    Str name = NULL;
    const char *p, *q;
    if ((p = httpGetHeader(buf->http_response, "Content-Disposition:")) !=
            NULL &&
        (q = strcasestr(p, "filename")) != NULL &&
        (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
        httpMatchattr(q, "filename", 8, &name))
      path = name->ptr;
    else if ((p = httpGetHeader(buf->http_response, "Content-Type:")) != NULL &&
             (q = strcasestr(p, "name")) != NULL &&
             (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
             httpMatchattr(q, "name", 4, &name))
      path = name->ptr;
  }
  return guess_filename(path);
}

struct Buffer *link_list_panel(struct Buffer *buf) {
  struct Url pu;
  /* FIXME: gettextize? */
  Str tmp =
      Strnew_charp("<title>Link List</title><h1 align=center>Link List</h1>\n");

  if (buf->bufferprop & BP_INTERNAL ||
      (buf->document->linklist == NULL && buf->document->href == NULL &&
       buf->document->img == NULL)) {
    return NULL;
  }

  const char *t;
  const char *u, *p;
  if (buf->document->linklist) {
    Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
    for (auto l = buf->document->linklist; l; l = l->next) {
      if (l->url) {
        parseURL2(l->url, &pu, baseURL(buf));
        p = parsedURL2Str(&pu)->ptr;
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

  if (buf->document->href) {
    Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
    auto al = buf->document->href;
    for (int i = 0; i < al->nanchor; i++) {
      auto a = &al->anchors[i];
      if (a->hseq < 0 || a->slave)
        continue;
      parseURL2(a->url, &pu, baseURL(buf));
      p = parsedURL2Str(&pu)->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      t = getAnchorText(buf->document, al, a);
      t = t ? html_quote(t) : "";
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  if (buf->document->img) {
    Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
    auto al = buf->document->img;
    for (int i = 0; i < al->nanchor; i++) {
      auto a = &al->anchors[i];
      if (a->slave)
        continue;
      parseURL2(a->url, &pu, baseURL(buf));
      p = parsedURL2Str(&pu)->ptr;
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
      a = retrieveAnchor(buf->document->formitem, a->start.line, a->start.pos);
      if (!a)
        continue;
      auto fi = (struct FormItemList *)a->url;
      fi = fi->parent->item;
      if (fi->parent->method == FORM_METHOD_INTERNAL &&
          !Strcmp_charp(fi->parent->action, "map") && fi->value) {
        struct MapList *ml = searchMapList(buf->document, fi->value->ptr);
        struct ListItem *mi;
        struct MapArea *m;
        if (!ml)
          continue;
        Strcat_charp(tmp, "<br>\n<b>Image map</b>\n<ol>\n");
        for (mi = ml->area->first; mi != NULL; mi = mi->next) {
          m = (struct MapArea *)mi->ptr;
          if (!m)
            continue;
          parseURL2(m->url, &pu, baseURL(buf));
          p = parsedURL2Str(&pu)->ptr;
          u = html_quote(p);
          if (DecodeURL)
            p = html_quote(url_decode0(p));
          else
            p = u;
          if (m->alt && *m->alt)
            t = html_quote(m->alt);
          else
            t = html_quote(url_decode0(m->url));
          Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
                         "\n", NULL);
        }
        Strcat_charp(tmp, "</ol>\n");
      }
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  return loadHTMLString(tmp);
}
