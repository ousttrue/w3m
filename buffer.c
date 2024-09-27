#include "buffer.h"
#include "app.h"
#include "url_stream.h"
#include "readbuffer.h"
#include "url.h"
#include "ctrlcode.h"
#include "url_stream.h"
#include "fm.h"
#include "tty.h"
#include "scr.h"
#include "termsize.h"
#include "terms.h"
#include <stdlib.h>
#include <unistd.h>

int REV_LB[MAX_LB] = {
    LB_N_INFO,
    LB_INFO,
    LB_N_SOURCE,
};
char *NullLine = "";
Lineprop NullProp[] = {0};

/*
 * Buffer creation
 */
struct Buffer *newBuffer(int width) {
  struct Buffer *n;

  n = New(struct Buffer);
  if (n == NULL)
    return NULL;
  memset((void *)n, 0, sizeof(struct Buffer));
  n->width = width;
  n->COLS = COLS;
  n->LINES = LASTLINE;
  n->currentURL.scheme = SCM_UNKNOWN;
  n->baseURL = NULL;
  n->baseTarget = NULL;
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
  struct Buffer *b;

  b = newBuffer(COLS);
  b->buffername = "*Null*";
  return b;
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(struct Buffer *buf) {
  buf->firstLine = buf->topLine = buf->currentLine = buf->lastLine = NULL;
  buf->allLine = 0;
}

/*
 * discardBuffer: free buffer structure
 */

void discardBuffer(struct Buffer *buf) {
  int i;
  struct Buffer *b;

  clearBuffer(buf);
  for (i = 0; i < MAX_LB; i++) {
    b = buf->linkBuffer[i];
    if (b == NULL)
      continue;
    b->linkBuffer[REV_LB[i]] = NULL;
  }
  if (buf->savecache)
    unlink(buf->savecache);
  if (--(*buf->clone))
    return;
  if (buf->sourcefile &&
      (!buf->real_type || strncasecmp(buf->real_type, "image/", 6))) {
    if (buf->real_scheme != SCM_LOCAL || buf->bufferprop & BP_FRAME)
      unlink(buf->sourcefile);
  }
  if (buf->header_source)
    unlink(buf->header_source);
  if (buf->mailcap_source)
    unlink(buf->mailcap_source);
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

  all = buf->allLine;
  if (all == 0 && buf->lastLine != NULL)
    all = buf->lastLine->linenumber;
  scr_move(n, 0);
  /* FIXME: gettextize? */
  msg = Sprintf("<%s> [%d lines]", buf->buffername, all);
  if (buf->filename != NULL) {
    switch (buf->currentURL.scheme) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
      if (strcmp(buf->currentURL.file, "-")) {
        Strcat_char(msg, ' ');
        Strcat_charp(msg, conv_from_system(buf->currentURL.real_file));
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
 * gotoLine: go to line number
 */
void gotoLine(struct Buffer *buf, int n) {
  char msg[32];
  Line *l = buf->firstLine;

  if (l == NULL)
    return;
  if (l->linenumber > n) {
    /* FIXME: gettextize? */
    sprintf(msg, "First line is #%ld", l->linenumber);
    set_delayed_message(msg);
    buf->topLine = buf->currentLine = l;
    return;
  }
  if (buf->lastLine->linenumber < n) {
    l = buf->lastLine;
    /* FIXME: gettextize? */
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
void gotoRealLine(struct Buffer *buf, int n) {
  char msg[32];
  Line *l = buf->firstLine;

  if (l == NULL)
    return;

  if (l->real_linenumber > n) {
    /* FIXME: gettextize? */
    sprintf(msg, "First line is #%ld", l->real_linenumber);
    set_delayed_message(msg);
    buf->topLine = buf->currentLine = l;
    return;
  }
  if (buf->lastLine->real_linenumber < n) {
    l = buf->lastLine;
    /* FIXME: gettextize? */
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
  struct URLFile f;
  struct Buffer sbuf;

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
  copyBuffer(&sbuf, buf);
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
    if (buf->currentURL.scheme != SCM_LOCAL || buf->mailcap_source ||
        !strcmp(buf->currentURL.file, "-")) {
      struct URLFile h;
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
  if (buf->firstLine && sbuf.firstLine) {
    Line *cur = sbuf.currentLine;
    int n;

    buf->pos = sbuf.pos + cur->bpos;
    while (cur->bpos && cur->prev)
      cur = cur->prev;
    if (cur->real_linenumber > 0)
      gotoRealLine(buf, cur->real_linenumber);
    else
      gotoLine(buf, cur->linenumber);
    n = (buf->currentLine->linenumber - buf->topLine->linenumber) -
        (cur->linenumber - sbuf.topLine->linenumber);
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
      buf->currentColumn = sbuf.currentColumn;
    arrangeCursor(buf);
  }
  if (buf->check_url & CHK_URL)
    chkURLBuffer(buf);
  formResetBuffer(buf, sbuf.formitem);
}

/* shallow copy */
void copyBuffer(struct Buffer *a, struct Buffer *b) {
  readBufferCache(b);
  memcpy((void *)a, (const void *)b, sizeof(struct Buffer));
}

struct Buffer *prevBuffer(struct Buffer *first, struct Buffer *buf) {
  struct Buffer *b;

  for (b = first; b != NULL && b->nextBuffer != buf; b = b->nextBuffer)
    ;
  return b;
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

int writeBufferCache(struct Buffer *buf) {
  Str tmp;
  FILE *cache = NULL;
  Line *l;

  if (buf->savecache)
    return -1;

  if (buf->firstLine == NULL)
    goto _error1;

  tmp = tmpfname(TMPF_CACHE, NULL);
  buf->savecache = tmp->ptr;
  cache = fopen(buf->savecache, "w");
  if (!cache)
    goto _error1;

  if (fwrite1(buf->currentLine->linenumber, cache) ||
      fwrite1(buf->topLine->linenumber, cache))
    goto _error;

  for (l = buf->firstLine; l; l = l->next) {
    if (fwrite1(l->real_linenumber, cache) || fwrite1(l->usrflags, cache) ||
        fwrite1(l->width, cache) || fwrite1(l->len, cache) ||
        fwrite1(l->size, cache) || fwrite1(l->bpos, cache) ||
        fwrite1(l->bwidth, cache))
      goto _error;
    if (l->bpos == 0) {
      if (fwrite(l->lineBuf, 1, l->size, cache) < l->size ||
          fwrite(l->propBuf, sizeof(Lineprop), l->size, cache) < l->size)
        goto _error;
    }
  }

  fclose(cache);
  return 0;
_error:
  fclose(cache);
  unlink(buf->savecache);
_error1:
  buf->savecache = NULL;
  return -1;
}

int readBufferCache(struct Buffer *buf) {
  FILE *cache;
  Line *l = NULL, *prevl = NULL, *basel = NULL;
  long lnum = 0, clnum, tlnum;

  if (buf->savecache == NULL)
    return -1;

  cache = fopen(buf->savecache, "r");
  if (cache == NULL || fread1(clnum, cache) || fread1(tlnum, cache)) {
    fclose(cache);
    buf->savecache = NULL;
    return -1;
  }

  while (!feof(cache)) {
    lnum++;
    prevl = l;
    l = New(Line);
    l->prev = prevl;
    if (prevl)
      prevl->next = l;
    else
      buf->firstLine = l;
    l->linenumber = lnum;
    if (lnum == clnum)
      buf->currentLine = l;
    if (lnum == tlnum)
      buf->topLine = l;
    if (fread1(l->real_linenumber, cache) || fread1(l->usrflags, cache) ||
        fread1(l->width, cache) || fread1(l->len, cache) ||
        fread1(l->size, cache) || fread1(l->bpos, cache) ||
        fread1(l->bwidth, cache))
      break;
    if (l->bpos == 0) {
      basel = l;
      l->lineBuf = NewAtom_N(char, l->size + 1);
      fread(l->lineBuf, 1, l->size, cache);
      l->lineBuf[l->size] = '\0';
      l->propBuf = NewAtom_N(Lineprop, l->size);
      fread(l->propBuf, sizeof(Lineprop), l->size, cache);
    } else if (basel) {
      l->lineBuf = basel->lineBuf + l->bpos;
      l->propBuf = basel->propBuf + l->bpos;
    } else
      break;
  }
  if (prevl) {
    buf->lastLine = prevl;
    buf->lastLine->next = NULL;
  }
  fclose(cache);
  unlink(buf->savecache);
  buf->savecache = NULL;
  return 0;
}

static void addnewline2(struct Buffer *buf, char *line, Lineprop *prop, int pos,
                        int nlines) {
  Line *l;
  l = New(Line);
  l->next = NULL;
  l->lineBuf = line;
  l->propBuf = prop;
  l->len = pos;
  l->width = -1;
  l->size = pos;
  l->bpos = 0;
  l->bwidth = 0;
  l->prev = buf->currentLine;
  if (buf->currentLine) {
    l->next = buf->currentLine->next;
    buf->currentLine->next = l;
  } else
    l->next = NULL;
  if (buf->lastLine == NULL || buf->lastLine == buf->currentLine)
    buf->lastLine = l;
  buf->currentLine = l;
  if (buf->firstLine == NULL)
    buf->firstLine = l;
  l->linenumber = ++buf->allLine;
  if (nlines < 0) {
    /*     l->real_linenumber = l->linenumber;     */
    l->real_linenumber = 0;
  } else {
    l->real_linenumber = nlines;
  }
  l = NULL;
}

void addnewline(struct Buffer *buf, char *line, Lineprop *prop, int pos,
                int width, int nlines) {
  char *s;
  Lineprop *p;
  Line *l;
  int i, bpos, bwidth;

  if (pos > 0) {
    s = allocStr(line, pos);
    p = NewAtom_N(Lineprop, pos);
    memcpy((void *)p, (const void *)prop, pos * sizeof(Lineprop));
  } else {
    s = NullLine;
    p = NullProp;
  }
  addnewline2(buf, s, p, pos, nlines);
  if (pos <= 0 || width <= 0)
    return;
  bpos = 0;
  bwidth = 0;
  while (1) {
    l = buf->currentLine;
    l->bpos = bpos;
    l->bwidth = bwidth;
    i = columnLen(l, width);
    if (i == 0) {
      i++;
    }
    l->len = i;
    l->width = COLPOS(l, l->len);
    if (pos <= i)
      return;
    bpos += l->len;
    bwidth += l->width;
    s += i;
    p += i;
    pos -= i;
    addnewline2(buf, s, p, pos, nlines);
  }
}
