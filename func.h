#pragma once
#include <wc.h>
#include <stdbool.h>

extern char* keymap_file;

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

typedef void (*CommandFunc)();

struct FuncList {
    const char* id;
    CommandFunc func;
};
// funcname.c
extern struct FuncList w3mFuncList[];

void initKeymap(wc_ces charset, wc_ces inner_charset, bool force);
int getFuncList(const char* id);
int getKey(const char* s);
char* getKeyData(int key);
struct regex;
char* getRegexWord(const char** str, struct regex** regex_ret);
void setKeymap(char* p, int lineno, int verbose);
