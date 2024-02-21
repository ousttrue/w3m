#pragma once
#define KEY_HASH_SIZE 127

extern unsigned char GlobalKeymap[];

extern int getFuncList(const char *id);
extern int getKey(const char *s);
extern const char *getKeyData(int key);
extern const char *getWord(const char **str);
extern const char *getQWord(const char **str);
extern void setKeymap(const char *p, int lineno, int verbose);
extern void initKeymap(int force);
void escKeyProc(int c, int esc, unsigned char *map);
