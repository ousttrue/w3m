#include "html_quote.h"
#include "quote.h"
#include "Str.h"
#include "entity.h"
#include <sstream>

const char *html_quote(const char *str) {
  Str *tmp = NULL;
  for (auto p = str; *p; p++) {
    auto q = html_quote_char(*p);
    if (q) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_charp(tmp, q);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return Strnew_charp(str)->ptr;
}

std::string html_unquote(std::string_view str) {
  std::stringstream tmp;

  for (auto p = str.data(); p != str.data() + str.size();) {
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
