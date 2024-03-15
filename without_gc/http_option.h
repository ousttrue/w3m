#pragma once
#include <string>

struct HttpOption {
  bool no_referer = false;
  bool no_cache = false;
  std::string referer;

  bool use_referer() const { return !no_referer && referer.size() > 0; }
};
