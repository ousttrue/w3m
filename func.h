#pragma once
#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

struct FuncList {
  const char *id;
  void (*func)();
};
extern FuncList w3mFuncList[];
extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];

extern int getFuncList(const char *id);
extern int getKey(const char *s);
extern const char *getKeyData(int key);
extern const char *getWord(const char **str);
extern const char *getQWord(const char **str);
extern void setKeymap(const char *p, int lineno, int verbose);
extern void initKeymap(int force);
