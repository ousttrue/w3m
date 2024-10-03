#pragma once
#include "Str.h"

struct Url;
struct URLFile;
struct Buffer;

bool http_matchattr(const char *p, const char *attr, int len, Str *value);
int http_readHeader(struct URLFile *uf, struct Buffer *newBuf, bool thru,
                    struct Url *pu);
const char *checkHeader(struct Buffer *buf, const char *field);
char *checkContentType(struct Buffer *buf);
