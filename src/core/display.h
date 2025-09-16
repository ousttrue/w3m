#pragma once
#include "geometry.h"
#include <Str.h>

extern int displayLink;
extern int displayLineInfo;
extern int FoldLine;

struct Buffer;
void bufToScreen(struct UI *ui);
void drawAnchorCursor(struct UI *ui);

struct Frame;
struct VirtualTerm;
struct Frame* screenToFrame(const struct VirtualTerm* vt);
