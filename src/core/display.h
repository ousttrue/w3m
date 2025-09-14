#pragma once
#include "geometry.h"
#include <Str.h>

extern int displayLink;
extern int displayLineInfo;
extern int FoldLine;

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))

struct Buffer;
void bufToScreen(struct UI ui);
void drawAnchorCursor(struct UI ui);

struct Frame;
struct VirtualTerm;
struct Frame* screenToFrame(const struct VirtualTerm* vt);
