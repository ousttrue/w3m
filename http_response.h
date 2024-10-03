#pragma once
#include "Str.h"

struct Url;
struct URLFile;
struct Buffer;

int matchattr(char *p, char *attr, int len, Str *value);
int readHeader(struct URLFile *uf, struct Buffer *newBuf, int thru,
               struct Url *pu);
const char *checkHeader(struct Buffer *buf, const char *field);
char *checkContentType(struct Buffer *buf);
