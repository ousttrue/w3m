#include "buffer/document.h"
#include "alloc.h"
#include "buffer/message.h"
#include "file/tmpfile.h"
#include "html/anchor.h"
#include "html/form.h"
#include "html/html_readbuffer.h"
#include "input/istream.h"
#include "term/termsize.h"
#include "text/text.h"
#include <string.h>

bool showLineNum = false;
bool FoldLine = false;
int FOLD_BUFFER_WIDTH() { return (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1); }
bool MarkAllPages = false;

struct Document *newDocument(int width) {
  struct Document *doc = New(struct Document);
  doc->url.scheme = SCM_UNKNOWN;
  doc->need_reshape = true; /* always reshape new buffers to mark URLs */
  doc->bufferprop = BP_NORMAL;
  doc->baseTarget = NULL;
  doc->width = width;
  doc->height = 0;
  doc->savecache = nullptr;
  doc->title = nullptr;
  doc->firstLine = nullptr;
  doc->topLine = nullptr;
  doc->currentLine = nullptr;
  doc->lastLine = nullptr;
  doc->allLine = 0;
  doc->href = nullptr;
  doc->name = nullptr;
  doc->img = nullptr;
  doc->formitem = nullptr;
  doc->linklist = nullptr;
  doc->formlist = nullptr;
  doc->maplist = nullptr;
  doc->hmarklist = nullptr;
  doc->imarklist = nullptr;
  doc->viewport.COLS = COLS;
  doc->viewport.LINES = LASTLINE;
  doc->viewport.currentColumn = 0;
  doc->viewport.cursorX = 0;
  doc->viewport.cursorY = 0;
  doc->viewport.pos = 0;
  doc->viewport.visualpos = 0;
  doc->viewport.rootX = 0;
  doc->viewport.rootY = 0;
  doc->viewport.undo = nullptr;
  doc->check_url = MarkAllPages; /* use default from -o mark_all_pages */

  return doc;
}

static void addnewline2(struct Document *doc, char *line, Lineprop *prop,
                        int pos, int nlines) {
  struct Line *l = New(struct Line);
  l->next = NULL;
  l->lineBuf = line;
  l->propBuf = prop;
  l->len = pos;
  l->width = -1;
  l->size = pos;
  l->bpos = 0;
  l->bwidth = 0;
  l->prev = doc->currentLine;
  if (doc->currentLine) {
    l->next = doc->currentLine->next;
    doc->currentLine->next = l;
  } else
    l->next = NULL;
  if (doc->lastLine == NULL || doc->lastLine == doc->currentLine)
    doc->lastLine = l;
  doc->currentLine = l;
  if (doc->firstLine == NULL)
    doc->firstLine = l;
  l->linenumber = ++doc->allLine;
  if (nlines < 0) {
    /*     l->real_linenumber = l->linenumber;     */
    l->real_linenumber = 0;
  } else {
    l->real_linenumber = nlines;
  }
  l = NULL;
}

char *NullLine = "";
Lineprop NullProp[] = {0};

void addnewline(struct Document *doc, char *line, Lineprop *prop, int pos,
                int width, int nlines) {
  char *s;
  Lineprop *p;
  // int i, bpos, bwidth;

  if (pos > 0) {
    s = allocStr(line, pos);
    p = NewAtom_N(Lineprop, pos);
    memcpy((void *)p, (const void *)prop, pos * sizeof(Lineprop));
  } else {
    s = NullLine;
    p = NullProp;
  }
  addnewline2(doc, s, p, pos, nlines);
  if (pos <= 0 || width <= 0)
    return;
  int bpos = 0;
  int bwidth = 0;
  while (1) {
    auto l = doc->currentLine;
    l->bpos = bpos;
    l->bwidth = bwidth;
    int i = columnLen(l, width);
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
    addnewline2(doc, s, p, pos, nlines);
  }
}

/*
 * gotoLine: go to line number
 */
void gotoLine(struct Document *doc, int n) {
  char msg[32];
  struct Line *l = doc->firstLine;

  if (l == NULL)
    return;
  if (l->linenumber > n) {
    sprintf(msg, "First line is #%ld", l->linenumber);
    message_push(msg);
    doc->topLine = doc->currentLine = l;
    return;
  }
  if (doc->lastLine->linenumber < n) {
    l = doc->lastLine;
    sprintf(msg, "Last line is #%ld", doc->lastLine->linenumber);
    message_push(msg);
    doc->currentLine = l;
    doc->topLine = lineSkip(&doc->viewport, doc->currentLine, doc->lastLine,
                            -(doc->viewport.LINES - 1), false);
    return;
  }
  for (; l != NULL; l = l->next) {
    if (l->linenumber >= n) {
      doc->currentLine = l;
      if (n < doc->topLine->linenumber ||
          doc->topLine->linenumber + doc->viewport.LINES <= n)
        doc->topLine = lineSkip(&doc->viewport, l, doc->lastLine,
                                -(doc->viewport.LINES + 1) / 2, false);
      break;
    }
  }
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void arrangeCursor(struct Document *doc) {
  int col, col2, pos;
  int delta = 1;
  if (doc == NULL || doc->currentLine == NULL)
    return;
  /* Arrange line */
  if (doc->currentLine->linenumber - doc->topLine->linenumber >=
          doc->viewport.LINES ||
      doc->currentLine->linenumber < doc->topLine->linenumber) {
    /*
     * doc->topLine = doc->currentLine;
     */
    doc->topLine =
        lineSkip(&doc->viewport, doc->currentLine, doc->lastLine, 0, false);
  }
  /* Arrange column */
  while (doc->viewport.pos < 0 && doc->currentLine->prev &&
         doc->currentLine->bpos) {
    pos = doc->viewport.pos + doc->currentLine->prev->len;
    cursorUp0(doc, 1);
    doc->viewport.pos = pos;
  }
  while (doc->viewport.pos >= doc->currentLine->len && doc->currentLine->next &&
         doc->currentLine->next->bpos) {
    pos = doc->viewport.pos - doc->currentLine->len;
    cursorDown0(doc, 1);
    doc->viewport.pos = pos;
  }
  if (doc->currentLine->len == 0 || doc->viewport.pos < 0)
    doc->viewport.pos = 0;
  else if (doc->viewport.pos >= doc->currentLine->len)
    doc->viewport.pos = doc->currentLine->len - 1;
  col = COLPOS(doc->currentLine, doc->viewport.pos);
  col2 = COLPOS(doc->currentLine, doc->viewport.pos + delta);
  if (col < doc->viewport.currentColumn ||
      col2 > doc->viewport.COLS + doc->viewport.currentColumn) {
    doc->viewport.currentColumn = 0;
    if (col2 > doc->viewport.COLS)
      columnSkip(doc, col);
  }
  /* Arrange cursor */
  doc->viewport.cursorY =
      doc->currentLine->linenumber - doc->topLine->linenumber;
  doc->viewport.visualpos = doc->currentLine->bwidth +
                            COLPOS(doc->currentLine, doc->viewport.pos) -
                            doc->viewport.currentColumn;
  doc->viewport.cursorX = doc->viewport.visualpos - doc->currentLine->bwidth;
#ifdef DISPLAY_DEBUG
  fprintf(
      stderr,
      "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
      doc->currentColumn, doc->cursorX, doc->visualpos, doc->pos,
      doc->currentLine->len);
#endif
}

void cursorUp0(struct Document *doc, int n) {
  if (doc->viewport.cursorY > 0)
    cursorUpDown(doc, -1);
  else {
    doc->topLine =
        lineSkip(&doc->viewport, doc->topLine, doc->lastLine, -n, false);
    if (doc->currentLine->prev != NULL)
      doc->currentLine = doc->currentLine->prev;
    arrangeLine(doc);
  }
}

void cursorUp(struct Document *doc, int n) {
  struct Line *l = doc->currentLine;
  if (doc->firstLine == NULL)
    return;
  while (doc->currentLine->prev && doc->currentLine->bpos)
    cursorUp0(doc, n);
  if (doc->currentLine == doc->firstLine) {
    gotoLine(doc, l->linenumber);
    arrangeLine(doc);
    return;
  }
  cursorUp0(doc, n);
  while (doc->currentLine->prev && doc->currentLine->bpos &&
         doc->currentLine->bwidth >=
             doc->viewport.currentColumn + doc->viewport.visualpos)
    cursorUp0(doc, n);
}

void cursorDown0(struct Document *doc, int n) {
  if (doc->viewport.cursorY < doc->viewport.LINES - 1)
    cursorUpDown(doc, 1);
  else {
    doc->topLine =
        lineSkip(&doc->viewport, doc->topLine, doc->lastLine, n, false);
    if (doc->currentLine->next != NULL)
      doc->currentLine = doc->currentLine->next;
    arrangeLine(doc);
  }
}

void cursorDown(struct Document *doc, int n) {
  struct Line *l = doc->currentLine;
  if (doc->firstLine == NULL)
    return;
  while (doc->currentLine->next && doc->currentLine->next->bpos)
    cursorDown0(doc, n);
  if (doc->currentLine == doc->lastLine) {
    gotoLine(doc, l->linenumber);
    arrangeLine(doc);
    return;
  }
  cursorDown0(doc, n);
  while (doc->currentLine->next && doc->currentLine->next->bpos &&
         doc->currentLine->bwidth + doc->currentLine->width <
             doc->viewport.currentColumn + doc->viewport.visualpos)
    cursorDown0(doc, n);
}

void cursorUpDown(struct Document *doc, int n) {
  struct Line *cl = doc->currentLine;
  if (doc->firstLine == NULL)
    return;
  if ((doc->currentLine = currentLineSkip(cl, n, false)) == cl)
    return;
  arrangeLine(doc);
}

void cursorRight(struct Document *doc, int n) {
  int i, delta = 1, cpos, vpos2;
  struct Line *l = doc->currentLine;
  Lineprop *p;

  if (doc->firstLine == NULL)
    return;
  if (doc->viewport.pos == l->len && !(l->next && l->next->bpos))
    return;
  i = doc->viewport.pos;
  p = l->propBuf;
  if (i + delta < l->len) {
    doc->viewport.pos = i + delta;
  } else if (l->len == 0) {
    doc->viewport.pos = 0;
  } else if (l->next && l->next->bpos) {
    cursorDown0(doc, 1);
    doc->viewport.pos = 0;
    arrangeCursor(doc);
    return;
  } else {
    doc->viewport.pos = l->len - 1;
  }
  cpos = COLPOS(l, doc->viewport.pos);
  doc->viewport.visualpos = l->bwidth + cpos - doc->viewport.currentColumn;
  delta = 1;
  vpos2 =
      COLPOS(l, doc->viewport.pos + delta) - doc->viewport.currentColumn - 1;
  if (vpos2 >= doc->viewport.COLS && n) {
    columnSkip(doc, n + (vpos2 - doc->viewport.COLS) -
                        (vpos2 - doc->viewport.COLS) % n);
    doc->viewport.visualpos = l->bwidth + cpos - doc->viewport.currentColumn;
  }
  doc->viewport.cursorX = doc->viewport.visualpos - l->bwidth;
}

void cursorLeft(struct Document *doc, int n) {
  int i, delta = 1, cpos;
  struct Line *l = doc->currentLine;
  Lineprop *p;

  if (doc->firstLine == NULL)
    return;
  i = doc->viewport.pos;
  p = l->propBuf;
  if (i >= delta)
    doc->viewport.pos = i - delta;
  else if (l->prev && l->bpos) {
    cursorUp0(doc, -1);
    doc->viewport.pos = doc->currentLine->len - 1;
    arrangeCursor(doc);
    return;
  } else
    doc->viewport.pos = 0;
  cpos = COLPOS(l, doc->viewport.pos);
  doc->viewport.visualpos = l->bwidth + cpos - doc->viewport.currentColumn;
  if (doc->viewport.visualpos - l->bwidth < 0 && n) {
    columnSkip(doc, -n + doc->viewport.visualpos - l->bwidth -
                        (doc->viewport.visualpos - l->bwidth) % n);
    doc->viewport.visualpos = l->bwidth + cpos - doc->viewport.currentColumn;
  }
  doc->viewport.cursorX = doc->viewport.visualpos - l->bwidth;
}

void cursorHome(struct Document *doc) {
  doc->viewport.visualpos = 0;
  doc->viewport.cursorX = doc->viewport.cursorY = 0;
}

void arrangeLine(struct Document *doc) {
  int i, cpos;

  if (doc->firstLine == NULL)
    return;
  doc->viewport.cursorY =
      doc->currentLine->linenumber - doc->topLine->linenumber;
  i = columnPos(doc->currentLine, doc->viewport.currentColumn +
                                      doc->viewport.visualpos -
                                      doc->currentLine->bwidth);
  cpos = COLPOS(doc->currentLine, i) - doc->viewport.currentColumn;
  if (cpos >= 0) {
    doc->viewport.cursorX = cpos;
    doc->viewport.pos = i;
  } else if (doc->currentLine->len > i) {
    doc->viewport.cursorX = 0;
    doc->viewport.pos = i + 1;
  } else {
    doc->viewport.cursorX = 0;
    doc->viewport.pos = 0;
  }
#ifdef DISPLAY_DEBUG
  fprintf(stderr,
          "arrangeLine: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
          doc->currentColumn, doc->cursorX, doc->visualpos, doc->pos,
          doc->currentLine->len);
#endif
}

void cursorXY(struct Document *doc, int x, int y) {
  int oldX;

  cursorUpDown(doc, y - doc->viewport.cursorY);

  if (doc->viewport.cursorX > x) {
    while (doc->viewport.cursorX > x)
      cursorLeft(doc, doc->viewport.COLS / 2);
  } else if (doc->viewport.cursorX < x) {
    while (doc->viewport.cursorX < x) {
      oldX = doc->viewport.cursorX;

      cursorRight(doc, doc->viewport.COLS / 2);

      if (oldX == doc->viewport.cursorX)
        break;
    }
    if (doc->viewport.cursorX > x)
      cursorLeft(doc, doc->viewport.COLS / 2);
  }
}

int columnSkip(struct Document *doc, int offset) {
  int i, maxColumn;
  int column = doc->viewport.currentColumn + offset;
  int nlines = doc->viewport.LINES + 1;
  struct Line *l;

  maxColumn = 0;
  for (i = 0, l = doc->topLine; i < nlines && l != NULL; i++, l = l->next) {
    if (l->width < 0)
      l->width = COLPOS(l, l->len);
    if (l->width - 1 > maxColumn)
      maxColumn = l->width - 1;
  }
  maxColumn -= doc->viewport.COLS - 1;
  if (column < maxColumn)
    maxColumn = column;
  if (maxColumn < 0)
    maxColumn = 0;

  if (doc->viewport.currentColumn == maxColumn)
    return 0;
  doc->viewport.currentColumn = maxColumn;
  return 1;
}

void restorePosition(struct Document *doc, struct Document *orig) {
  doc->topLine = lineSkip(&doc->viewport, doc->firstLine, doc->lastLine,
                          TOP_LINENUMBER(orig) - 1, false);
  gotoLine(doc, CUR_LINENUMBER(orig));
  doc->viewport.pos = orig->viewport.pos;
  if (doc->currentLine && orig->currentLine)
    doc->viewport.pos += orig->currentLine->bpos - doc->currentLine->bpos;
  doc->viewport.currentColumn = orig->viewport.currentColumn;
  arrangeCursor(doc);
}

#define fwrite1(d, f) (fwrite(&d, sizeof(d), 1, f) == 0)
#define fread1(d, f) (fread(&d, sizeof(d), 1, f) == 0)

int writeBufferCache(struct Document *doc) {
  if (doc->savecache)
    return -1;

  if (doc->firstLine == NULL)
    goto _error1;

  auto tmp = tmpfname(TMPF_CACHE, NULL);
  doc->savecache = tmp->ptr;
  auto cache = fopen(doc->savecache, "w");
  if (!cache)
    goto _error1;

  if (fwrite1(doc->currentLine->linenumber, cache) ||
      fwrite1(doc->topLine->linenumber, cache))
    goto _error;

  for (auto l = doc->firstLine; l; l = l->next) {
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
  unlink(doc->savecache);
_error1:
  doc->savecache = NULL;
  return -1;
}

int readBufferCache(struct Document *doc) {
  if (doc->savecache == NULL)
    return -1;

  auto cache = fopen(doc->savecache, "r");
  long lnum = 0, clnum, tlnum;
  if (cache == NULL || fread1(clnum, cache) || fread1(tlnum, cache)) {
    fclose(cache);
    doc->savecache = NULL;
    return -1;
  }

  struct Line *l = NULL, *prevl = NULL, *basel = NULL;
  while (!feof(cache)) {
    lnum++;
    prevl = l;
    l = New(struct Line);
    l->prev = prevl;
    if (prevl)
      prevl->next = l;
    else
      doc->firstLine = l;
    l->linenumber = lnum;
    if (lnum == clnum)
      doc->currentLine = l;
    if (lnum == tlnum)
      doc->topLine = l;
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
    doc->lastLine = prevl;
    doc->lastLine->next = NULL;
  }
  fclose(cache);
  unlink(doc->savecache);
  doc->savecache = NULL;
  return 0;
}

void tmpClearBuffer(struct Document *doc) {
  if (writeBufferCache(doc) == 0) {
    doc->firstLine = NULL;
    doc->topLine = NULL;
    doc->currentLine = NULL;
    doc->lastLine = NULL;
  }
}

/* shallow copy */
void copyBuffer(struct Document *a, const struct Document *b) {
  readBufferCache((struct Document *)b);
  memcpy((void *)a, (const void *)b, sizeof(struct Document));
}

void COPY_BUFROOT(struct Document *dstbuf, const struct Document *srcbuf) {
  (dstbuf)->viewport.rootX = (srcbuf)->viewport.rootX;
  (dstbuf)->viewport.rootY = (srcbuf)->viewport.rootY;
  (dstbuf)->viewport.COLS = (srcbuf)->viewport.COLS;
  (dstbuf)->viewport.LINES = (srcbuf)->viewport.LINES;
}

void COPY_BUFPOSITION(struct Document *dstbuf, const struct Document *srcbuf) {
  (dstbuf)->topLine = (srcbuf)->topLine;
  (dstbuf)->currentLine = (srcbuf)->currentLine;
  (dstbuf)->viewport.pos = (srcbuf)->viewport.pos;
  (dstbuf)->viewport.cursorX = (srcbuf)->viewport.cursorX;
  (dstbuf)->viewport.cursorY = (srcbuf)->viewport.cursorY;
  (dstbuf)->viewport.visualpos = (srcbuf)->viewport.visualpos;
  (dstbuf)->viewport.currentColumn = (srcbuf)->viewport.currentColumn;
}

int currentLn(struct Document *doc) {
  if (doc->currentLine)
    /*     return buf->currentLine->real_linenumber + 1;      */
    return doc->currentLine->linenumber + 1;
  else
    return 1;
}

/*
 * gotoRealLine: go to real line number
 */
void gotoRealLine(struct Document *doc, int n) {
  auto l = doc->firstLine;
  if (!l)
    return;

  char msg[32];
  if (l->real_linenumber > n) {
    sprintf(msg, "First line is #%ld", l->real_linenumber);
    message_push(msg);
    doc->topLine = doc->currentLine = l;
    return;
  }

  if (doc->lastLine->real_linenumber < n) {
    l = doc->lastLine;
    sprintf(msg, "Last line is #%ld", doc->lastLine->real_linenumber);
    message_push(msg);
    doc->currentLine = l;
    doc->topLine = lineSkip(&doc->viewport, doc->currentLine, doc->lastLine,
                            -(doc->viewport.LINES - 1), false);
    return;
  }

  for (; l; l = l->next) {
    if (l->real_linenumber >= n) {
      doc->currentLine = l;
      if (n < doc->topLine->real_linenumber ||
          doc->topLine->real_linenumber + doc->viewport.LINES <= n)
        doc->topLine = lineSkip(&doc->viewport, l, doc->lastLine,
                                -(doc->viewport.LINES + 1) / 2, false);
      break;
    }
  }
}

/*
 * clearBuffer: clear buffer content
 */
void clearBuffer(struct Document *doc) {
  doc->firstLine = doc->topLine = doc->currentLine = doc->lastLine = NULL;
  doc->allLine = 0;
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(struct Document *doc) {
  static char *url_like_pat[] = {
      "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/"
      "=\\-]",
      "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
      "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
      "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
      "https?://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./"
      "?=~_\\&+@#,\\$;]*",
      "ftp://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
      NULL};
  int i;
  for (i = 0; url_like_pat[i]; i++) {
    reAnchor(doc, url_like_pat[i]);
  }
  doc->check_url |= CHK_URL;
}

/*
 * Reshape HTML buffer
 */
struct Document *reshapeBuffer(struct Document *doc,
                               enum CharSet content_charset) {
  if (!doc->need_reshape) {
    return doc;
  }
  doc->need_reshape = false;

  doc->width = INIT_BUFFER_WIDTH;
  if (doc->sourcefile == NULL) {
    return doc;
  }

  auto stream = examineFile(doc->sourcefile);
  if (stream == NULL) {
    return doc;
  }

  Str html = Strnew();
  Str line;
  while ((line = StrmyISgets(stream))->length) {
    Strcat(html, line);
  }

  struct Document sbuf;
  copyBuffer(&sbuf, doc);
  clearBuffer(doc);

  doc->href = NULL;
  doc->name = NULL;
  doc->img = NULL;
  doc->formitem = NULL;
  doc->formlist = NULL;
  doc->linklist = NULL;
  doc->maplist = NULL;
  if (doc->hmarklist)
    doc->hmarklist->nmark = 0;
  if (doc->imarklist)
    doc->imarklist->nmark = 0;

  struct Document *newDoc;
  if (is_html_type(doc->type)) {
    newDoc =
        renderHTML(doc->viewport.COLS, html->ptr, doc->url, content_charset);
  } else {
    newDoc = loadText(doc->viewport.COLS, html->ptr);
  }
  ISclose(stream);

  newDoc->height = LASTLINE + 1;
  if (newDoc->firstLine && sbuf.firstLine) {
    struct Line *cur = sbuf.currentLine;
    int n;

    newDoc->viewport.pos = sbuf.viewport.pos + cur->bpos;
    while (cur->bpos && cur->prev)
      cur = cur->prev;
    if (cur->real_linenumber > 0)
      gotoRealLine(newDoc, cur->real_linenumber);
    else
      gotoLine(newDoc, cur->linenumber);
    n = (newDoc->currentLine->linenumber - newDoc->topLine->linenumber) -
        (cur->linenumber - sbuf.topLine->linenumber);
    if (n) {
      newDoc->topLine = lineSkip(&newDoc->viewport, newDoc->topLine,
                                 newDoc->lastLine, n, false);
      if (cur->real_linenumber > 0)
        gotoRealLine(newDoc, cur->real_linenumber);
      else
        gotoLine(newDoc, cur->linenumber);
    }
    newDoc->viewport.pos -= newDoc->currentLine->bpos;
    if (FoldLine && !is_html_type(newDoc->type))
      newDoc->viewport.currentColumn = 0;
    else
      newDoc->viewport.currentColumn = sbuf.viewport.currentColumn;
    arrangeCursor(newDoc);
  }
  if (newDoc->check_url & CHK_URL)
    chkURLBuffer(newDoc);
  formResetBuffer(newDoc, sbuf.formitem);
  return newDoc;
}

/* Go to specified line */
void _goLine(struct Document *doc, const char *l) {
  if (l == NULL || *l == '\0' || doc->currentLine == NULL) {
    // displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }

  doc->viewport.pos = 0;
  // if (((*l == '^') || (*l == '$')) && prec_num) {
  //   gotoRealLine(doc, prec_num);
  // } else
  if (*l == '^') {
    doc->topLine = doc->currentLine = doc->firstLine;
  } else if (*l == '$') {
    doc->topLine = lineSkip(&doc->viewport, doc->lastLine, doc->lastLine,
                            -(doc->viewport.LINES + 1) / 2, true);
    doc->currentLine = doc->lastLine;
  } else
    gotoRealLine(doc, atoi(l));
  arrangeCursor(doc);
  // displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void save_buffer_position(struct Document *doc) {
  if (!doc->firstLine)
    return;

  struct BufferPos *b = doc->viewport.undo;
  if (b && b->top_linenumber == TOP_LINENUMBER(doc) &&
      b->cur_linenumber == CUR_LINENUMBER(doc) &&
      b->currentColumn == doc->viewport.currentColumn &&
      b->pos == doc->viewport.pos)
    return;

  b = New(struct BufferPos);
  b->top_linenumber = TOP_LINENUMBER(doc);
  b->cur_linenumber = CUR_LINENUMBER(doc);
  b->currentColumn = doc->viewport.currentColumn;
  b->pos = doc->viewport.pos;
  b->bpos = doc->currentLine ? doc->currentLine->bpos : 0;
  b->next = NULL;
  b->prev = doc->viewport.undo;
  if (doc->viewport.undo)
    doc->viewport.undo->next = b;
  doc->viewport.undo = b;
}

void resetPos(struct Document *doc, struct BufferPos *b) {
  struct Line top;
  top.linenumber = b->top_linenumber;
  struct Line cur;
  cur.linenumber = b->cur_linenumber;
  cur.bpos = b->bpos;
  struct Document _doc;
  _doc.topLine = &top;
  _doc.currentLine = &cur;
  _doc.viewport.pos = b->pos;
  _doc.viewport.currentColumn = b->currentColumn;
  restorePosition(doc, &_doc);
  doc->viewport.undo = b;
  // displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

int prev_nonnull_line(struct Document *doc, struct Line *line) {
  struct Line *l;
  for (l = line; l != NULL && l->len == 0; l = l->prev)
    ;
  if (l == NULL || l->len == 0)
    return -1;

  doc->currentLine = l;
  if (l != line)
    doc->viewport.pos = doc->currentLine->len;
  return 0;
}

struct Url *baseURL(struct Document *doc) {
  if (doc->bufferprop & BP_NO_URL) {
    /* no URL is defined for the buffer */
    return NULL;
  }

  // if (doc->baseURL != NULL) {
  //   /* <BASE> tag is defined in the document */
  //   return doc->baseURL;
  // } else
  if (IS_EMPTY_PARSED_URL(&doc->url))
    return NULL;
  else
    return &doc->url;
}
