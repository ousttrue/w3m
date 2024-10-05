#include "line.h"
#include "alloc.h"
#include "myctype.h"
#include "utf8.h"

int Tabstop = 8;
int ShowEffect = true;

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

int columnLen(struct Line *line, int column) {
  int i, j;

  for (i = 0, j = 0; i < line->len;) {
    j = nextColumn(j, &line->lineBuf[i], &line->propBuf[i]);
    if (j > column)
      return i;
    i++;
  }
  return line->len;
}

int columnPos(struct Line *line, int column) {
  int i;

  for (i = 1; i < line->len; i++) {
    if (COLPOS(line, i) > column)
      break;
  }
  return i - 1;
}

Str checkType(Str s, Lineprop **oprop) {
  Lineprop mode;
  Lineprop effect = PE_NORMAL;
  Lineprop *prop;
  static Lineprop *prop_buffer = NULL;
  static int prop_size = 0;
  char *str = s->ptr, *endp = &s->ptr[s->length], *bs = NULL;
  bool do_copy = false;
  int i;
  int plen = 0, clen;

  if (prop_size < s->length) {
    prop_size = (s->length > LINELEN) ? s->length : LINELEN;
    prop_buffer = New_Reuse(Lineprop, prop_buffer, prop_size);
  }
  prop = prop_buffer;

  if (ShowEffect) {
    bs = memchr(str, '\b', s->length);
    if ((bs != NULL)) {
      char *sp = str, *ep;
      s = Strnew_size(s->length);
      do_copy = true;
      ep = bs ? (bs - 2) : endp;
      for (; str < ep && IS_ASCII(*str); str++) {
        *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
      }
      Strcat_charp_n(s, sp, (int)(str - sp));
    }
  }
  if (!do_copy) {
    for (; str < endp && IS_ASCII(*str); str++)
      *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
  }

  while (str < endp) {
    if (prop - prop_buffer >= prop_size)
      break;
    if (bs != NULL) {
      if (str == bs - 1 && *str == '_') {
        str += 2;
        effect = PE_UNDER;
        if (str < endp)
          bs = memchr(str, '\b', endp - str);
        continue;
      } else if (str == bs) {
        if (*(str + 1) == '_') {
          if (s->length) {
            str += 2;
            *(prop - 1) |= PE_UNDER;
          } else {
            str++;
          }
        } else {
          if (s->length) {
            if (*(str - 1) == *(str + 1)) {
              *(prop - 1) |= PE_BOLD;
              str += 2;
            } else {
              Strshrink(s, 1);
              prop--;
              str++;
            }
          } else {
            str++;
          }
        }
        if (str < endp)
          bs = memchr(str, '\b', endp - str);
        continue;
      }
    }

    plen = utf8sequence_len(str);
    mode = get_mctype(str) | effect;
    *prop = mode;
    if (do_copy)
      Strcat_char(s, (char)*str);

    prop += plen;
    str += plen;
    effect = PE_NORMAL;
  }
  *oprop = prop_buffer;
  return s;
}

void clear_mark(struct Line *l) {
  int pos;
  if (!l)
    return;
  for (pos = 0; pos < l->size; pos++)
    l->propBuf[pos] &= ~PE_MARK;
}
