#pragma once
#include <string>

extern bool UseAltEntity;

/// unicode codepoint or ISO-8859 to utf8 ?
std::string conv_entity(char32_t ch);
char32_t getescapechar(const char **s);
std::string getescapecmd(const char **s);

// return [matched, remain]
std::tuple<std::string_view, std::string_view>
matchNamedCharacterReference(std::string_view src);
