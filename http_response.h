#pragma once
#include "Str.h"
#include "line.h"

extern int ShowEffect;

struct Url;
struct URLFile;
struct Buffer;

int matchattr(char *p, char *attr, int len, Str *value);
Str checkType(Str s, Lineprop **oprop);
int readHeader(struct URLFile *uf, struct Buffer *newBuf, int thru,
               struct Url *pu);
char *checkHeader(struct Buffer *buf, char *field);
char *checkContentType(struct Buffer *buf);
