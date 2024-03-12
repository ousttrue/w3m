#pragma once
#include <string>
#include <string_view>

extern bool DecodeURL;

std::string file_quote(std::string_view str);
std::string file_unquote(std::string_view str);

std::string Str_url_unquote(std::string_view url, bool is_form, bool safe);
inline std::string Str_form_unquote(std::string_view url) {
  return Str_url_unquote(url, true, false);
}
inline std::string url_unquote_conv0(std::string_view url) {
  return Str_url_unquote(url, false, true);
}

inline std::string url_decode0(std::string_view url) {
  if (!DecodeURL) {
    return {url.begin(), url.end()};
  }
  return url_unquote_conv0(url);
}
