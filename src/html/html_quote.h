#pragma once
#include <string>
#include <string_view>

const char *html_quote(const char *str);
std::string html_unquote(std::string_view str);
