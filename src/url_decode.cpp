#include "url_decode.h"
#include "quote.h"
#include "myctype.h"
#include <sstream>

bool DecodeURL = false;

static char url_unquote_char(const char **pstr) {
  return ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2]))
              ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) |
                                   GET_MYCDIGIT((*(pstr))[-1]))
              : -1);
}

std::string file_quote(std::string_view str) {
  std::stringstream tmp;
  for (auto p = str.begin(); p != str.end(); ++p) {
    if (is_file_quote(*p)) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)*p);
      tmp << buf;
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}

std::string file_unquote(std::string_view str) {
  std::stringstream tmp;
  for (auto p = str.data(); p != str.data() + str.size();) {
    if (*p == '%') {
      auto q = &*p;
      int c = url_unquote_char(&q);
      if (c >= 0) {
        if (c != '\0' && c != '\n' && c != '\r') {
          tmp << (char)c;
        }
        p = q;
        continue;
      }
    }
    tmp << *p;
    p++;
  }
  return tmp.str();
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
