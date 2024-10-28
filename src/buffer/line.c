#include "buffer/line.h"
#include "alloc.h"
#include "text/myctype.h"
#include "text/utf8.h"
#include <string.h>

#define LINELEN 256 /* Initial line length */

int Tabstop = 8;
int ShowEffect = true;

enum CharTypes CharType(Lineprop c) { return (enum CharTypes)((c)&P_CHARTYPE); }
enum CharEffects CharEffect(Lineprop c) {
  return (enum CharEffects)((c) & (P_EFFECT | PC_SYMBOL));
}
// void SetCharType(Lineprop *v, enum CharTypes c) { *v = (*v & ~P_CHARTYPE) | c; }

int get_mctype(const uint8_t *c) {
  return (IS_CNTRL(*(c)) ? PC_CTRL : PC_ASCII);
}

int COLPOS(struct Line *l, int c) {
  return calcPosition(l->lineBuf, l->propBuf, l->len, c, CP_AUTO);
}

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

int calcPosition(char *l, Lineprop *pr, int len, int pos,
                 enum ColumnPositionMode mode) {
  static int *realColumn = nullptr;
  static int size = 0;
  static char *prevl = nullptr;
  if (l == nullptr || len == 0 || pos < 0) {
    return 0;
  }
  if (l == prevl && mode == CP_AUTO) {
    // cache
    if (pos <= len) {
      return realColumn[pos];
    }
  }

  if (size < len + 1) {
    size = (len + 1 > LINELEN) ? (len + 1) : LINELEN;
    realColumn = New_N(int, size);
  }
  prevl = l;
  int i = 0;
  int j = 0;
  while (1) {
    realColumn[i] = j;
    if (i == len)
      break;
    j = nextColumn(j, &l[i], &pr[i]);
    i++;
  }
  if (pos >= i) {
    return j;
  }
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
  int i = 0;
  int j = 0;
  for (; i < line->len;) {
    auto len = utf8sequence_len((const uint8_t*)&line->lineBuf[i]);
    auto col = utf8sequence_width((const uint8_t*)&line->lineBuf[i]);
    if (j + col > column) {
      break;
    }
    i += len;
    j += col;
  }
  return i;
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
