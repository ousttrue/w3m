#include "line.h"
#include "alloc.h"

int Tabstop = 8;

static int nextColumn(int n, char *p, Lineprop *pr) {
  if (*pr & PC_CTRL) {
    if (*p == '\t')
      return (n + Tabstop) / Tabstop * Tabstop;
    else if (*p == '\n')
      return n + 1;
    else if (*p != '\r')
      return n + 2;
    return n;
  }
  return n + 1;
}

int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos,
                 enum ColumnPositionMode mode) {
  static int *realColumn = nullptr;
  static int size = 0;
  static char *prevl = nullptr;
  int i, j;

  if (l == nullptr || len == 0 || pos < 0)
    return bpos;
  if (l == prevl && mode == CP_AUTO) {
    if (pos <= len)
      return realColumn[pos];
  }
  if (size < len + 1) {
    size = (len + 1 > LINELEN) ? (len + 1) : LINELEN;
    realColumn = New_N(int, size);
  }
  prevl = l;
  i = 0;
  j = bpos;
  while (1) {
    realColumn[i] = j;
    if (i == len)
      break;
    j = nextColumn(j, &l[i], &pr[i]);
    i++;
  }
  if (pos >= i)
    return j;
  return realColumn[pos];
}

int columnLen(Line *line, int column) {
  int i, j;

  for (i = 0, j = 0; i < line->len;) {
    j = nextColumn(j, &line->lineBuf[i], &line->propBuf[i]);
    if (j > column)
      return i;
    i++;
  }
  return line->len;
}

int columnPos(Line *line, int column) {
  int i;

  for (i = 1; i < line->len; i++) {
    if (COLPOS(line, i) > column)
      break;
  }
  return i - 1;
}
