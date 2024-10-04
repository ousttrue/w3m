#pragma once

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

typedef struct _FuncList {
  char *id;
  void (*func)();
} FuncList;
extern FuncList w3mFuncList[];

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];

int getFuncList(const char *id);
void setKeymap(const char *p, int lineno, int verbose);
int getKey(const char *s);

void pcmap(void);
void escmap(void);
void escbmap(void);
void escdmap(char c);
void multimap(void);
void initKeymap(int force);

char *getKeyData(int key);
