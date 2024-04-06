#pragma once
#include <string>
#include <optional>

extern bool UseAltEntity;

/// unicode codepoint or ISO-8859 to utf8 ?
std::string conv_entity(char32_t ch);

/// return [codepoint?, remain]
std::tuple<std::optional<char32_t>, std::string_view>
getescapechar(std::string_view s);

/// return [cmd, remain]
std::tuple<std::string, std::string_view> getescapecmd(std::string_view s);

/// return [matched, remain]
std::tuple<std::string_view, std::string_view>
matchNamedCharacterReference(std::string_view src);
