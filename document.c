#include "document.h"
#include "alloc.h"
#include "terms.h"

bool nextpage_topline = false;

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
