#pragma once
#include "textlist.h"
#include "hash.h"

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

void setKeymap(char* p, int lineno, int verbose);
void initKeymap(int force);
int getKey(char* s);
char* getKeyData(int key);
char* getWord(char** str);
char* getQWord(char** str);
struct regex;
char* getRegexWord(const char** str, struct regex** regex_ret);
