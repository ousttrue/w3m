#pragma once
#include "line.h"
struct _Buffer;
struct Frame;
struct VirtualTerm;

void bufToScreen(struct VirtualTerm* vt, struct _Buffer* buf);
struct Frame* screenToFrame(const struct VirtualTerm* vt);

void drawAnchorCursor(struct _Buffer* buf);
Str make_lastline_message(struct _Buffer* buf);

void addChar(char c, Lineprop mode);
void addMChar(char* c, Lineprop mode, size_t len);

void redrawNLine(struct _Buffer* buf, int n);
Line* redrawLine(struct _Buffer* buf, Line* l, int i);
Line* redrawLineImage(struct _Buffer* buf, Line* l, int i);
int redrawLineRegion(struct _Buffer* buf, Line* l, int i, int bpos, int epos);

