#pragma once
#include <string>

extern const char *w3m_version;

struct HttpOption {
  bool no_referer = false;
  bool no_cache = false;
  std::string referer;
  std::string w3m_version = ::w3m_version;

  bool use_referer() const { return !no_referer && referer.size() > 0; }
};
