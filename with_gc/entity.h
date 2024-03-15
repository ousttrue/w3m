#pragma once
#include <string>

std::string conv_entity(char32_t ch);

int getescapechar(const char **s);

std::string getescapecmd(const char **s);
