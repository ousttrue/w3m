#pragma once
#include "Str.h"

struct Url;
struct URLFile;
struct Buffer;

int http_readHeader(struct URLFile *uf, struct Buffer *newBuf, struct Url *pu);
bool http_matchattr(const char *p, const char *attr, int len, Str *value);
const char *checkHeader(struct Buffer *buf, const char *field);
char *checkContentType(struct Buffer *buf);
