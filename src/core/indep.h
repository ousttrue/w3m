#pragma once
#include <alloc.h>
#include <Str.h>
#include <stdbool.h>

extern long long strtoclen(const char* s);
extern char* currentdir(void);
extern const char* expandPath(const char* name);
extern int strcasemstr(char* str, char* srch[], char** ret_ptr);
int strmatchlen(const char* s1, const char* s2, int maxlen);

