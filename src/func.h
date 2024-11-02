#pragma once
#include "current.h"

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

typedef struct _FuncList {
  const char *id;
  void (*func)(struct Current);
} FuncList;
extern FuncList w3mFuncList[];

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];

int getFuncList(const char *id);
void setKeymap(const char *p, int lineno);
int getKey(const char *s);

void pcmap(struct Current);
void escmap(struct Current);
void escbmap(struct Current);
void escdmap(char c);
void multimap(struct Current);
void initKeymap(int force);

char *getKeyData(int key);
