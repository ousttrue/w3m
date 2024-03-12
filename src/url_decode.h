#pragma once
#include <string>
#include <string_view>

extern bool DecodeURL;

std::string file_quote(std::string_view str);
std::string file_unquote(std::string_view str);

struct Str *Str_url_unquote(Str *x, int is_form, int safe);
inline Str *Str_form_unquote(Str *x) { return Str_url_unquote(x, true, false); }

const char *url_decode0(const char *url);
