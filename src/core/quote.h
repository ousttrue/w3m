#pragma once

char* getWord(char** str);
char* getQWord(char** str);

struct regex;
char* getRegexWord(const char** str, struct regex** regex_ret);
