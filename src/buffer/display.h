#pragma once
#include "term/termsize.h"

extern bool displayLineInfo;
extern bool displayLink;
extern int enable_inline_image;

struct Buffer;
void display(struct Buffer *buf, struct TermSize size);
void reshapeBuffer(struct Buffer *buf, struct TermSize size);
