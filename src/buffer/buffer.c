#include "buffer/buffer.h"
#include "alloc.h"
#include "buffer/document.h"
#include "file/file.h"
#include "fm.h"
#include "history.h"
#include "html/html_readbuffer.h"
#include "html/html_text.h"
#include "html/map.h"
#include "input/http.h"
#include "input/istream.h"
#include "input/loader.h"
#include "input/url.h"
#include "linein.h"
#include "message.h"
#include "siteconf.h"
#include "term/scr.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "term/tty.h"
#include "text/ctrlcode.h"
#include "text/text.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
  n->buffername = "";
  n->clone = New(int);
  *n->clone = 1;
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
 * discardBuffer: free buffer structure
 */
void discardBuffer(struct Buffer *buf) {
  clearBuffer(buf->document);
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

  if (buf->document->sourcefile &&
      (!buf->document->type || strncasecmp(buf->document->type, "image/", 6))) {
    if (buf->document->real_scheme != SCM_LOCAL ||
        buf->document->bufferprop & BP_FRAME)
      unlink(buf->document->sourcefile);
  }
}

/*
 * namedBuffer: Select buffer which have specified name
 */
struct Buffer *namedBuffer(struct Buffer *first, char *name) {
  if (!strcmp(first->buffername, name)) {
    return first;
  }
  for (auto buf = first; buf->nextBuffer != NULL; buf = buf->nextBuffer) {
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

  if (first == delbuf && first->nextBuffer != NULL) {
    auto buf = first->nextBuffer;
    discardBuffer(first);
    return buf;
  }

  struct Buffer *buf = prevBuffer(first, delbuf);
  if (buf) {
    auto b = buf->nextBuffer;
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
  if (n < 0)
    return firstbuf;

  struct Buffer *buf = firstbuf;
  for (int i = 0; i < n; i++) {
    if (buf == NULL)
      return NULL;
    buf = buf->nextBuffer;
  }
  return buf;
}

static void writeBufferName(struct Buffer *buf, int n) {
  auto all = buf->document->allLine;
  if (all == 0 && buf->document->lastLine != NULL)
    all = buf->document->lastLine->linenumber;
  scr_move(n, 0);
  auto msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
  if (buf->document->filename != NULL) {
    switch (buf->document->url.scheme) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
      if (strcmp(buf->document->url.file, "-")) {
        Strcat_char(msg, ' ');
        Strcat_charp(msg, buf->document->url.real_file);
      }
      break;
    case SCM_UNKNOWN:
    case SCM_MISSING:
      break;
    default:
      Strcat_char(msg, ' ');
      Strcat(msg, parsedURL2Str(&buf->document->url));
      break;
    }
  }
  scr_addnstr_sup(msg->ptr, COLS - 1);
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
  } else if (buf->document->url.scheme == SCM_LOCAL) {
    if (stat(buf->document->url.file, &st) < 0)
      return "unknown";
    return ctime(&st.st_mtime);
  }
  return "unknown";
}

struct Document *link_list_panel(struct Buffer *buf) {
  Str tmp =
      Strnew_charp("<title>Link List</title><h1 align=center>Link List</h1>\n");

  if (buf->document->bufferprop & BP_INTERNAL ||
      (buf->document->linklist == NULL && buf->document->href == NULL &&
       buf->document->img == NULL)) {
    return NULL;
  }

  if (buf->document->linklist) {
    Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
    for (auto l = buf->document->linklist; l; l = l->next) {
      const char *p;
      const char *u;
      if (l->url) {
        auto pu = parseURL2(l->url, baseURL(buf->document));
        p = parsedURL2Str(&pu)->ptr;
        u = html_quote(p);
        if (DecodeURL)
          p = html_quote(url_decode0(p));
        else
          p = u;
      } else {
        u = p = "";
      }
      const char *t;
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
      auto pu = parseURL2(a->url, baseURL(buf->document));
      const char *p = parsedURL2Str(&pu)->ptr;
      auto u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      const char *t = getAnchorText(buf->document, al, a);
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
      auto pu = parseURL2(a->url, baseURL(buf->document));
      const char *p = parsedURL2Str(&pu)->ptr;
      auto u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      const char *t;
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
          pu = parseURL2(m->url, baseURL(buf->document));
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

  struct Url url;
  return renderHTML(INIT_BUFFER_WIDTH, tmp->ptr, url, CHARSET_UTF8);
}

struct Buffer *loadLink(struct Document *doc, const char *url,
                        const char *target, const char *referer,
                        struct FormList *form) {
  // scr_message(Sprintf("loading %s", url)->ptr, 0, 0);
  // term_refresh();
  auto no_referer_ptr = query_SCONF_NO_REFERER_FROM(&doc->url);
  auto base = baseURL(doc);
  if ((no_referer_ptr && *no_referer_ptr) || base == NULL ||
      base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI ||
      base->scheme == SCM_DATA)
    referer = NO_REFERER;
  if (referer == NULL)
    referer = parsedURL2RefererStr(&doc->url)->ptr;
  int flag = 0;
  auto buf = loadGeneralFile(INIT_BUFFER_WIDTH, url, baseURL(doc), referer,
                             flag, form);
  if (buf == NULL) {
    // char *emsg = Sprintf("Can't load %s", url)->ptr;
    // message_push(emsg);
    return NULL;
  }

  auto pu = parseURL2(url, base);
  pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

  if (buf == NO_BUFFER) {
    return NULL;
  }
  // if (!on_target) /* open link as an indivisual page */
  //   return loadNormalBuf(buf);
  return buf;

  // if (target == NULL || /* no target specified (that means this page is not a
  //                          frame page) */
  //     !strcmp(target, "_top") ||    /* this link is specified to be opened as
  //     an
  //                                      indivisual * page */
  //     !(doc->bufferprop & BP_FRAME) /* This page is not a frame page */
  // ) {
  //   return loadNormalBuf(buf);
  // }
  // /* original page (that contains <frameset> tag) doesn't exist */
  // return loadNormalBuf(buf);
}

static struct FormItemList *save_submit_formlist(struct FormItemList *src) {
  struct FormList *list;
  struct FormList *srclist;
  struct FormItemList *srcitem;
  struct FormItemList *item;
  struct FormItemList *ret = NULL;

  if (src == NULL)
    return NULL;
  srclist = src->parent;
  list = New(struct FormList);
  list->method = srclist->method;
  list->action = Strdup(srclist->action);
  list->enctype = srclist->enctype;
  list->nitems = srclist->nitems;
  list->body = srclist->body;
  list->boundary = srclist->boundary;
  list->length = srclist->length;

  for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
    item = New(struct FormItemList);
    item->type = srcitem->type;
    item->name = Strdup(srcitem->name);
    item->value = Strdup(srcitem->value);
    item->checked = srcitem->checked;
    item->accept = srcitem->accept;
    item->size = srcitem->size;
    item->rows = srcitem->rows;
    item->maxlength = srcitem->maxlength;
    item->readonly = srcitem->readonly;
    item->parent = list;
    item->next = NULL;

    if (list->lastitem == NULL) {
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

struct Buffer *_followForm(struct Document *doc, bool submit, struct Current current) {
  if (doc->firstLine == NULL)
    return nullptr;

  auto a = retrieveCurrentForm(doc);
  if (a == NULL)
    return nullptr;

  auto fi = (struct FormItemList *)a->url;

  switch (fi->type) {
  case FORM_INPUT_TEXT: {
    if (submit)
      goto do_submit;
    if (fi->readonly) {
      message_push("Read only field!");
    }
    auto p =
        inputStrHist(doc, "TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
    if (p == NULL || fi->readonly)
      break;
    fi->value = Strnew_charp(p);
    formUpdateBuffer(doc, a, fi);
    if (fi->accept || fi->parent->nitems == 1)
      goto do_submit;
    break;
  }

  case FORM_INPUT_FILE: {
    if (submit)
      goto do_submit;
    if (fi->readonly)
      message_push("Read only field!");
    auto p = inputFilenameHist(
        doc, "Filename:", fi->value ? fi->value->ptr : NULL, NULL);
    if (p == NULL || fi->readonly)
      break;
    fi->value = Strnew_charp(p);
    formUpdateBuffer(doc, a, fi);
    if (fi->accept || fi->parent->nitems == 1)
      goto do_submit;
    break;
  }

  case FORM_INPUT_PASSWORD: {
    if (submit)
      goto do_submit;
    if (fi->readonly) {
      message_push("Read only field!");
      break;
    }
    auto p = inputLine(doc, "Password:", fi->value ? fi->value->ptr : NULL,
                       IN_PASSWORD);
    if (p == NULL)
      break;
    fi->value = Strnew_charp(p);
    formUpdateBuffer(doc, a, fi);
    if (fi->accept)
      goto do_submit;
    break;
  }

  case FORM_TEXTAREA:
    if (submit)
      goto do_submit;
    if (fi->readonly)
      message_push("Read only field!");
    input_textarea(fi);
    formUpdateBuffer(doc, a, fi);
    break;

  case FORM_INPUT_RADIO:
    if (submit)
      goto do_submit;
    if (fi->readonly) {
      message_push("Read only field!");
      break;
    }
    formRecheckRadio(doc, a, fi);
    break;

  case FORM_INPUT_CHECKBOX:
    if (submit)
      goto do_submit;
    if (fi->readonly) {
      message_push("Read only field!");
      break;
    }
    fi->checked = !fi->checked;
    formUpdateBuffer(doc, a, fi);
    break;

  case FORM_INPUT_IMAGE:
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON: {
  do_submit:
    auto tmp = Strnew();
    auto multipart = (fi->parent->method == FORM_METHOD_POST &&
                      fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
    query_from_followform(&tmp, fi, multipart);

    auto tmp2 = Strdup(fi->parent->action);
    if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
      /* It means "current URL" */
      tmp2 = parsedURL2Str(&doc->url);
      char *p;
      if ((p = strchr(tmp2->ptr, '?')) != NULL)
        Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    }

    if (fi->parent->method == FORM_METHOD_GET) {
      char *p;
      if ((p = strchr(tmp2->ptr, '?')) != NULL)
        Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
      Strcat_charp(tmp2, "?");
      Strcat(tmp2, tmp);
      return loadLink(doc, tmp2->ptr, a->target, NULL, NULL);
    } else if (fi->parent->method == FORM_METHOD_POST) {
      if (multipart) {
        struct stat st;
        stat(fi->parent->body, &st);
        fi->parent->length = st.st_size;
      } else {
        fi->parent->body = tmp->ptr;
        fi->parent->length = tmp->length;
      }
      auto buf = loadLink(doc, tmp2->ptr, a->target, NULL, fi->parent);
      if (multipart) {
        unlink(fi->parent->body);
      }
      if (buf && !(buf->document->bufferprop &
                   BP_REDIRECTED)) { /* buf must be Currentbuf */
        /* BP_REDIRECTED means that the buffer is obtained through
         * Location: header. In this case, buf->form_submit must not be set
         * because the page is not loaded by POST method but GET method.
         */
        buf->document->form_submit = save_submit_formlist(fi);
      }

      return buf;
    } else if ((fi->parent->method == FORM_METHOD_INTERNAL &&
                (!Strcmp_charp(fi->parent->action, "map") ||
                 !Strcmp_charp(fi->parent->action, "none"))) ||
               doc->bufferprop & BP_INTERNAL) { /* internal */
      do_internal(tmp2->ptr, tmp->ptr, current);
    } else {
      message_push("Can't send form because of illegal method.");
    }
    break;
  }

  case FORM_INPUT_RESET:
    for (int i = 0; i < doc->formitem->nanchor; i++) {
      auto a2 = &doc->formitem->anchors[i];
      auto f2 = (struct FormItemList *)a2->url;
      if (f2->parent == fi->parent && f2->name && f2->value &&
          f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN &&
          f2->type != FORM_INPUT_RESET) {
        f2->value = f2->init_value;
        f2->checked = f2->init_checked;
        formUpdateBuffer(doc, a2, f2);
      }
    }
    break;
  case FORM_INPUT_HIDDEN:
  default:
    break;
  }

  return nullptr;
}
