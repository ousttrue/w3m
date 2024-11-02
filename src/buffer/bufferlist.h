#pragma once
#include "term/termsize.h"

struct Buffer;
struct Buffer *selectBuffer(struct Buffer *firstbuf, struct Buffer *currentbuf,
                            struct TermSize size, char *selectchar);
