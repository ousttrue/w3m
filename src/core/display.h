#pragma once
#include "line.h"
#include "ui.h"
#include <Str.h>

extern int displayLink;
extern int displayLineInfo;
extern int FoldLine;
extern int enable_inline_image;
extern int displayImage;

#define DEFAULT_PIXEL_PER_CHAR 7.0 /* arbitrary */
#define DEFAULT_PIXEL_PER_LINE 14.0 /* arbitrary */
#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

extern double pixel_per_char;
extern int pixel_per_char_i;
extern int set_pixel_per_char;
extern double pixel_per_line;
extern int pixel_per_line_i;
extern int set_pixel_per_line;

#define REAL_WIDTH(w, limit) (((w) >= 0) ? (int)((w) / pixel_per_char) : -(w) * (limit) / 100)
#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))

struct _Buffer;
void bufToScreen(struct UI ui, struct _Buffer* buf);
void drawAnchorCursor(struct UI ui, struct _Buffer* buf);

struct Frame;
struct VirtualTerm;
struct Frame* screenToFrame(const struct VirtualTerm* vt);
