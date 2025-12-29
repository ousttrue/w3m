#pragma once
#include "textlist.h"
#include "hash.h"

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

struct FuncList {
    char* id;
    void (*func)();
};
struct regex;

char* getWord(char** str);
char* getRegexWord(const char** str, struct regex** regex_ret);
char* getQWord(char** str);
int getFuncList(const char* id);
