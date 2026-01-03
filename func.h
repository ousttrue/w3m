#pragma once
#include "defun.h"
#include <stdbool.h>

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];

struct FuncList {
    const char* id;
    DefunFunc func;
};
extern struct FuncList w3mFuncList[];
extern void setKeymap(const char* p, int lineno, bool verbose);

/// event
int getFuncList(const char* id);
void initKeymap(int force);
int getKey(char* s);
char* getKeyData(int key);
