#pragma once
#include <locale>
#include <codecvt>
#include <string>

inline std::string codepoint_to_utf8(char32_t codepoint) {
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
  return convert.to_bytes(&codepoint, &codepoint + 1);
}
