#include "url_decode.h"
#include "quote.h"
#include "myctype.h"
#include <sstream>

bool DecodeURL = false;

char url_unquote_char(const char **pstr) {
  return ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2]))
              ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) |
                                   GET_MYCDIGIT((*(pstr))[-1]))
              : -1);
}

std::string Str_url_unquote(std::string_view x, bool is_form, bool safe) {
  std::stringstream tmp;
  auto p = x.data();
  auto ep = x.data() + x.size();
  for (; p < ep;) {
    if (is_form && *p == '+') {
      tmp << ' ';
      p++;
      continue;
    } else if (*p == '%') {
      auto q = p;
      int c = url_unquote_char(&q);
      if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c))) {
        tmp << (char)c;
        p = q;
        continue;
      }
    }
    tmp << *p;
    p++;
  }
  return tmp.str();
}
