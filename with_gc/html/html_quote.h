#pragma once
#include "quote.h"
#include "Str.h"

inline const char *html_quote(const char *str) {
  return Strnew(html_quote(std::string_view(str)))->ptr;
}
