#pragma once
#include "line.h"
#include <Str.h>

extern int displayLink;
extern int displayLineInfo;
extern int FoldLine;
extern int showLineNum;
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

struct _Buffer;
struct Frame;
struct VirtualTerm;

void bufToScreen(struct VirtualTerm* vt, struct _Buffer* buf, bool use_graphic);
struct Frame* screenToFrame(const struct VirtualTerm* vt);

void drawAnchorCursor(struct _Buffer* buf, bool use_graphic);
Str make_lastline_message(struct _Buffer* buf);

void redrawNLine(struct _Buffer* buf, int n, bool use_graphic);
Line* redrawLine(struct _Buffer* buf, Line* l, int i, bool use_graphic);
Line* redrawLineImage(struct _Buffer* buf, Line* l, int i);
int redrawLineRegion(struct _Buffer* buf, Line* l, int i, int bpos, int epos, bool use_graphic);
