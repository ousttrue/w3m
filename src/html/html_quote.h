#pragma once
#include <string>
#include <string_view>
#include "Str.h"

std::string html_quote(std::string_view str);
std::string html_unquote(std::string_view str);

inline const char *html_quote(const char *str) {
  return Strnew(html_quote(std::string_view(str)))->ptr;
}
