#include "lineprop.h"
#include "alloc.h"
#include "utf8.h"
#include "myctype.h"
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

Lineprop get_mctype(const char *_c) {
  auto c = *_c;
  if (c <= 0x7F) {
    if (!IS_CNTRL(c)) {
      return PC_ASCII;
    }
  }

  // TODO
  return PC_CTRL;
}
