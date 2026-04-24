#pragma once
#include <stdint.h>
#include <stdbool.h>

extern char* conv_entity(unsigned int ch);
extern int getescapechar(char** s);
extern char* getescapecmd(char** s);
extern char* currentdir(void);
extern char* cleanupName(const char* name);
extern char* expandPath(const char* name);
extern char* remove_space(const char* str);
extern char* html_quote(const char* str);
extern char* html_unquote(const char* str);
extern char* file_quote(const char* str);
extern char* file_unquote(const char* str);
extern char* shell_quote(const char* str);

