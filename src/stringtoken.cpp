#include "stringtoken.h"

std::optional<std::string_view> stringtoken::sloppy_parse_line() {
  if (is_end()) {
    return {};
  }

  if (current() == '<') {
    auto begin = ptr();
    for (; !is_end(); next()) {
      if (current() == '>') {
        break;
      }
    }
    if (!is_end() && current() == '>') {
      next();
    }
    return std::string_view{begin, ptr()};
  } else {
    for (; !is_end(); next()) {
      if (current() == '<') {
        break;
      }
    }
    return {};
  }
}
