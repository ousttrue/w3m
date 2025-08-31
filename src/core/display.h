#pragma once
#include "line.h"
struct _Buffer;
struct Frame;
struct VirtualTerm;

void renderToScreen();
struct Frame* screenToFrame(const struct VirtualTerm* vt);

void drawAnchorCursor(struct _Buffer* buf);
Str make_lastline_message(struct _Buffer* buf);

void addChar(char c, Lineprop mode);
void addMChar(char* c, Lineprop mode, size_t len);
struct _Buffer* message_list_panel(void);

void set_delayed_message(char* s);

