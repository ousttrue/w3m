#pragma once
#define KEY_HASH_SIZE 127


extern int getKey(const char *s);
extern const char *getWord(const char **str);
extern const char *getQWord(const char **str);
extern void initKeymap(int force);
void escKeyProc(int c, int esc, unsigned char *map);
