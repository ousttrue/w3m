#include "document.h"
#include "alloc.h"
#include "terms.h"
#include "termsize.h"
#include "tmpfile.h"

bool nextpage_topline = false;

struct Document *newDocument(int width) {
  struct Document *doc = New(struct Document);
  doc->baseTarget = NULL;
  doc->baseURL = NULL;
  doc->width = width;
  doc->height = 0;
  doc->COLS = COLS;
  doc->LINES = LASTLINE;
  doc->savecache = nullptr;
  doc->title = nullptr;
  doc->firstLine = nullptr;
  doc->topLine = nullptr;
  doc->currentLine = nullptr;
  doc->lastLine = nullptr;
  doc->allLine = 0;
  doc->currentColumn = 0;
  doc->cursorX = 0;
  doc->cursorY = 0;
  doc->pos = 0;
  doc->visualpos = 0;
  doc->rootX = 0;
  doc->rootY = 0;
  doc->href = nullptr;
  doc->name = nullptr;
  doc->img = nullptr;
  doc->formitem = nullptr;
  doc->linklist = nullptr;
  doc->formlist = nullptr;
  doc->maplist = nullptr;
  doc->hmarklist = nullptr;
  doc->imarklist = nullptr;
  doc->undo = nullptr;
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

struct Line *currentLineSkip(struct Line *line, int offset, int last) {
  struct Line *l = line;
  if (offset == 0)
    return l;
  if (offset > 0)
    for (int i = 0; i < offset && l->next != NULL; i++, l = l->next)
      ;
  else
    for (int i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
      ;
  return l;
}

struct Line *lineSkip(struct Document *doc, struct Line *line, int offset,
                      int last) {
  auto l = currentLineSkip(line, offset, last);
  if (!nextpage_topline)
    for (int i = doc->LINES - 1 - (doc->lastLine->linenumber - l->linenumber);
         i > 0 && l->prev != NULL; i--, l = l->prev)
      ;
  return l;
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
    /* FIXME: gettextize? */
    sprintf(msg, "First line is #%ld", l->linenumber);
    set_delayed_message(msg);
    doc->topLine = doc->currentLine = l;
    return;
  }
  if (doc->lastLine->linenumber < n) {
    l = doc->lastLine;
    /* FIXME: gettextize? */
    sprintf(msg, "Last line is #%ld", doc->lastLine->linenumber);
    set_delayed_message(msg);
    doc->currentLine = l;
    doc->topLine = lineSkip(doc, doc->currentLine, -(doc->LINES - 1), false);
    return;
  }
  for (; l != NULL; l = l->next) {
    if (l->linenumber >= n) {
      doc->currentLine = l;
      if (n < doc->topLine->linenumber ||
          doc->topLine->linenumber + doc->LINES <= n)
        doc->topLine = lineSkip(doc, l, -(doc->LINES + 1) / 2, false);
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
  if (doc->currentLine->linenumber - doc->topLine->linenumber >= doc->LINES ||
      doc->currentLine->linenumber < doc->topLine->linenumber) {
    /*
     * doc->topLine = doc->currentLine;
     */
    doc->topLine = lineSkip(doc, doc->currentLine, 0, false);
  }
  /* Arrange column */
  while (doc->pos < 0 && doc->currentLine->prev && doc->currentLine->bpos) {
    pos = doc->pos + doc->currentLine->prev->len;
    cursorUp0(doc, 1);
    doc->pos = pos;
  }
  while (doc->pos >= doc->currentLine->len && doc->currentLine->next &&
         doc->currentLine->next->bpos) {
    pos = doc->pos - doc->currentLine->len;
    cursorDown0(doc, 1);
    doc->pos = pos;
  }
  if (doc->currentLine->len == 0 || doc->pos < 0)
    doc->pos = 0;
  else if (doc->pos >= doc->currentLine->len)
    doc->pos = doc->currentLine->len - 1;
  col = COLPOS(doc->currentLine, doc->pos);
  col2 = COLPOS(doc->currentLine, doc->pos + delta);
  if (col < doc->currentColumn || col2 > doc->COLS + doc->currentColumn) {
    doc->currentColumn = 0;
    if (col2 > doc->COLS)
      columnSkip(doc, col);
  }
  /* Arrange cursor */
  doc->cursorY = doc->currentLine->linenumber - doc->topLine->linenumber;
  doc->visualpos = doc->currentLine->bwidth +
                   COLPOS(doc->currentLine, doc->pos) - doc->currentColumn;
  doc->cursorX = doc->visualpos - doc->currentLine->bwidth;
#ifdef DISPLAY_DEBUG
  fprintf(
      stderr,
      "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
      doc->currentColumn, doc->cursorX, doc->visualpos, doc->pos,
      doc->currentLine->len);
#endif
}

void cursorUp0(struct Document *doc, int n) {
  if (doc->cursorY > 0)
    cursorUpDown(doc, -1);
  else {
    doc->topLine = lineSkip(doc, doc->topLine, -n, false);
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
         doc->currentLine->bwidth >= doc->currentColumn + doc->visualpos)
    cursorUp0(doc, n);
}

void cursorDown0(struct Document *doc, int n) {
  if (doc->cursorY < doc->LINES - 1)
    cursorUpDown(doc, 1);
  else {
    doc->topLine = lineSkip(doc, doc->topLine, n, false);
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
             doc->currentColumn + doc->visualpos)
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
  if (doc->pos == l->len && !(l->next && l->next->bpos))
    return;
  i = doc->pos;
  p = l->propBuf;
  if (i + delta < l->len) {
    doc->pos = i + delta;
  } else if (l->len == 0) {
    doc->pos = 0;
  } else if (l->next && l->next->bpos) {
    cursorDown0(doc, 1);
    doc->pos = 0;
    arrangeCursor(doc);
    return;
  } else {
    doc->pos = l->len - 1;
  }
  cpos = COLPOS(l, doc->pos);
  doc->visualpos = l->bwidth + cpos - doc->currentColumn;
  delta = 1;
  vpos2 = COLPOS(l, doc->pos + delta) - doc->currentColumn - 1;
  if (vpos2 >= doc->COLS && n) {
    columnSkip(doc, n + (vpos2 - doc->COLS) - (vpos2 - doc->COLS) % n);
    doc->visualpos = l->bwidth + cpos - doc->currentColumn;
  }
  doc->cursorX = doc->visualpos - l->bwidth;
}

void cursorLeft(struct Document *doc, int n) {
  int i, delta = 1, cpos;
  struct Line *l = doc->currentLine;
  Lineprop *p;

  if (doc->firstLine == NULL)
    return;
  i = doc->pos;
  p = l->propBuf;
  if (i >= delta)
    doc->pos = i - delta;
  else if (l->prev && l->bpos) {
    cursorUp0(doc, -1);
    doc->pos = doc->currentLine->len - 1;
    arrangeCursor(doc);
    return;
  } else
    doc->pos = 0;
  cpos = COLPOS(l, doc->pos);
  doc->visualpos = l->bwidth + cpos - doc->currentColumn;
  if (doc->visualpos - l->bwidth < 0 && n) {
    columnSkip(doc, -n + doc->visualpos - l->bwidth -
                        (doc->visualpos - l->bwidth) % n);
    doc->visualpos = l->bwidth + cpos - doc->currentColumn;
  }
  doc->cursorX = doc->visualpos - l->bwidth;
}

void cursorHome(struct Document *doc) {
  doc->visualpos = 0;
  doc->cursorX = doc->cursorY = 0;
}

void arrangeLine(struct Document *doc) {
  int i, cpos;

  if (doc->firstLine == NULL)
    return;
  doc->cursorY = doc->currentLine->linenumber - doc->topLine->linenumber;
  i = columnPos(doc->currentLine,
                doc->currentColumn + doc->visualpos - doc->currentLine->bwidth);
  cpos = COLPOS(doc->currentLine, i) - doc->currentColumn;
  if (cpos >= 0) {
    doc->cursorX = cpos;
    doc->pos = i;
  } else if (doc->currentLine->len > i) {
    doc->cursorX = 0;
    doc->pos = i + 1;
  } else {
    doc->cursorX = 0;
    doc->pos = 0;
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

  cursorUpDown(doc, y - doc->cursorY);

  if (doc->cursorX > x) {
    while (doc->cursorX > x)
      cursorLeft(doc, doc->COLS / 2);
  } else if (doc->cursorX < x) {
    while (doc->cursorX < x) {
      oldX = doc->cursorX;

      cursorRight(doc, doc->COLS / 2);

      if (oldX == doc->cursorX)
        break;
    }
    if (doc->cursorX > x)
      cursorLeft(doc, doc->COLS / 2);
  }
}

int columnSkip(struct Document *doc, int offset) {
  int i, maxColumn;
  int column = doc->currentColumn + offset;
  int nlines = doc->LINES + 1;
  struct Line *l;

  maxColumn = 0;
  for (i = 0, l = doc->topLine; i < nlines && l != NULL; i++, l = l->next) {
    if (l->width < 0)
      l->width = COLPOS(l, l->len);
    if (l->width - 1 > maxColumn)
      maxColumn = l->width - 1;
  }
  maxColumn -= doc->COLS - 1;
  if (column < maxColumn)
    maxColumn = column;
  if (maxColumn < 0)
    maxColumn = 0;

  if (doc->currentColumn == maxColumn)
    return 0;
  doc->currentColumn = maxColumn;
  return 1;
}

void restorePosition(struct Document *doc, struct Document *orig) {
  doc->topLine = lineSkip(doc, doc->firstLine, TOP_LINENUMBER(orig) - 1, false);
  gotoLine(doc, CUR_LINENUMBER(orig));
  doc->pos = orig->pos;
  if (doc->currentLine && orig->currentLine)
    doc->pos += orig->currentLine->bpos - doc->currentLine->bpos;
  doc->currentColumn = orig->currentColumn;
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
  readBufferCache(b);
  memcpy((void *)a, (const void *)b, sizeof(struct Document));
}

void COPY_BUFROOT(struct Document *dstbuf, const struct Document *srcbuf) {
  (dstbuf)->rootX = (srcbuf)->rootX;
  (dstbuf)->rootY = (srcbuf)->rootY;
  (dstbuf)->COLS = (srcbuf)->COLS;
  (dstbuf)->LINES = (srcbuf)->LINES;
}

void COPY_BUFPOSITION(struct Document *dstbuf, const struct Document *srcbuf) {
  (dstbuf)->topLine = (srcbuf)->topLine;
  (dstbuf)->currentLine = (srcbuf)->currentLine;
  (dstbuf)->pos = (srcbuf)->pos;
  (dstbuf)->cursorX = (srcbuf)->cursorX;
  (dstbuf)->cursorY = (srcbuf)->cursorY;
  (dstbuf)->visualpos = (srcbuf)->visualpos;
  (dstbuf)->currentColumn = (srcbuf)->currentColumn;
}

int currentLn(struct Document *doc) {
  if (doc->currentLine)
    /*     return buf->currentLine->real_linenumber + 1;      */
    return doc->currentLine->linenumber + 1;
  else
    return 1;
}
