#pragma once
#include <memory>
#include <string_view>

class HtmlTag;
std::shared_ptr<HtmlTag> parseHtmlTag(const char **p, bool internal);

inline std::shared_ptr<HtmlTag> parseHtmlTag(std::string_view v,
                                             bool internal) {
  std::string s(v.begin(), v.end());
  auto p = s.c_str();
  return parseHtmlTag(&p, internal);
}
