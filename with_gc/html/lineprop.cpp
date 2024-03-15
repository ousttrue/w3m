#include "lineprop.h"
#include "html/readbuffer.h"
#include "Str.h"
#include "myctype.h"
#include "alloc.h"
#include "utf8.h"
#include <vector>

int Tabstop = 8;

int bytePosToColumn(const char *l, const Lineprop *pr, int len, int pos,
                    int bpos, bool forceCalc) {
  if (!l || len == 0 || pos < 0)
    return bpos;

  static std::vector<int> realColumn;
  static const char *prevl = {};
  if (l == prevl && !forceCalc) {
    if (pos <= len)
      return realColumn[pos];
  }

  realColumn.clear();
  prevl = l;
  int i = 0;
  int j = bpos;
  for (; i < len;) {
    if (pr[i] & PC_CTRL) {
      realColumn.push_back(j);
      if (l[i] == '\t')
        j += Tabstop - (j % Tabstop);
      else if (l[i] == '\n')
        j += 1;
      else if (l[i] != '\r')
        j += 2;
      ++i;
    } else {
      auto utf8 = Utf8::from((const char8_t *)&l[i]);
      auto [cp, bytes] = utf8.codepoint();
      if (bytes) {
        for (int k = 0; k < bytes; ++k) {
          realColumn.push_back(j);
          ++i;
        }
      } else {
        realColumn.push_back(j);
        ++i;
      }
      j += utf8.width();
    }
  }
  realColumn.push_back(j);
  if (pos < static_cast<int>(realColumn.size())) {
    return realColumn[pos];
  }
  return j;
}

/*
 * Check character type
 */

Str *checkType(Str *s, Lineprop **oprop) {
  static Lineprop *prop_buffer = NULL;
  static int prop_size = 0;

  char *str = s->ptr;
  char *endp = &s->ptr[s->length];
  char *bs = NULL;

  if (prop_size < s->length) {
    prop_size = (s->length > LINELEN) ? s->length : LINELEN;
    prop_buffer = (Lineprop *)New_Reuse(Lineprop, prop_buffer, prop_size);
  }
  auto prop = prop_buffer;

  bool do_copy = false;
  if (!do_copy) {
    for (; str < endp && IS_ASCII(*str); str++) {
      *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
    }
  }

  Lineprop effect = PE_NORMAL;
  while (str < endp) {
    if (prop - prop_buffer >= prop_size)
      break;
    if (bs != NULL) {
      if (str == bs - 1 && *str == '_') {
        str += 2;
        effect = PE_UNDER;
        if (str < endp)
          bs = (char *)memchr(str, '\b', endp - str);
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
          bs = (char *)memchr(str, '\b', endp - str);
        continue;
      }
    }

    auto mode = get_mctype(str) | effect;
    *(prop++) = mode;
    {
      if (do_copy)
        Strcat_char(s, (char)*str);
      str++;
    }
    effect = PE_NORMAL;
  }
  *oprop = prop_buffer;
  return s;
}
