#include "html_text.h"
#include "Str.h"
#include "myctype.h"

bool is_html_quote(char c) { return (GET_QUOTE_TYPE(c) & HTML_QUOTE_MASK); }

const char *HTML_QUOTE_MAP[] = {
    NULL, "&amp;", "&lt;", "&gt;", "&quot;", "&apos;", NULL, NULL,
};

const char *html_quote(const char *str) {
  Str tmp = NULL;
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
  return str;
}
