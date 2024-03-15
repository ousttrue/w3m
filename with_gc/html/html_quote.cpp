#include "html_quote.h"
#include "quote.h"
#include "entity.h"
#include <sstream>

std::string html_quote(std::string_view str) {
  std::stringstream tmp;
  auto end = str.data() + str.size();
  for (auto p = str.data(); p != end; p++) {
    auto q = html_quote_char(*p);
    if (q) {
      tmp << q;
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}

std::string html_unquote(std::string_view str) {
  std::stringstream tmp;
  auto end = str.data() + str.size();
  for (auto p = str.data(); p != end;) {
    if (*p == '&') {
      auto q = getescapecmd(&p);
      tmp << q;
    } else {
      tmp << *p;
      p++;
    }
  }
  return tmp.str();
}
