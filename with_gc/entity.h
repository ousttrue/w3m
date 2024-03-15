#pragma once
#include <string>

extern char UseAltEntity;

/// unicode codepoint or ISO-8859 to utf8 ?
std::string conv_entity(char32_t ch);
int getescapechar(const char **s);
std::string getescapecmd(const char **s);
