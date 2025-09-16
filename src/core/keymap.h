#pragma once
#include "geometry.h"

#define KEY_HASH_SIZE 127

#define K_ESC 0x100
#define K_ESCB 0x200
#define K_ESCD 0x400
#define K_MULTI 0x10000000
#define MULTI_KEY(c) (((c) >> 16) & 0x77F)

extern char* keymap_file;

typedef void (*CommandFunc)(struct UI ui);
extern CommandFunc GlobalKeymap[];
extern CommandFunc EscKeymap[];
extern CommandFunc EscBKeymap[];
extern CommandFunc EscDKeymap[];

struct FuncList {
    const char* id;
    CommandFunc func;
};
extern struct FuncList w3mFuncList[];

void setKeymap(struct UI ui, const char* p, int lineno);
void initKeymap(struct UI ui, int force);
CommandFunc getFunc(const char* id);
int getKey(const char* s);
char* getKeyData(int key);
void addFunc(CommandFunc func, const char *name, const char *desc); 
