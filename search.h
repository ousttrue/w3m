#pragma once

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

struct Buffer;
extern int forwardSearch(struct Buffer *buf, char *str);
extern int backwardSearch(struct Buffer *buf, char *str);
