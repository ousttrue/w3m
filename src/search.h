#pragma once

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

struct Buffer;
int forwardSearch(Buffer *buf, const char *str);
int backwardSearch(Buffer *buf, const char *str);

int srchcore(const char *str, int (*func)(Buffer *, const char *));
