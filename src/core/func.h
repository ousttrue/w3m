#pragma once

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

typedef void (*CommandFunc)();

typedef struct _FuncList {
    const char* id;
    CommandFunc func;
} FuncList;

extern char* searchKeyData(void);
extern void setKeymap(char* p, int lineno);
extern void initKeymap(int force);
extern int getFuncList(char* id);
extern int getKey(char* s);
extern char* getKeyData(int key);
extern char* getWord(char** str);
extern char* getQWord(char** str);
struct regex;
extern char* getRegexWord(const char** str, struct regex** regex_ret);
