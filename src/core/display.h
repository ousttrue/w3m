#pragma once
#include "line.h"
struct _Buffer;

struct Frame;
struct Frame* displayBuffer();

struct VirtualTerm;
struct Frame* screenToFrame(const struct VirtualTerm* vt);

void addChar(char c, Lineprop mode);
void addMChar(char* c, Lineprop mode, size_t len);
struct _Buffer* message_list_panel(void);

void set_delayed_message(char* s);
void cursorUp0(struct _Buffer* buf, int n);
void cursorUp(struct _Buffer* buf, int n);
void cursorDown0(struct _Buffer* buf, int n);
void cursorDown(struct _Buffer* buf, int n);
void cursorUpDown(struct _Buffer* buf, int n);
void cursorRight(struct _Buffer* buf, int n);
void cursorLeft(struct _Buffer* buf, int n);
void cursorHome(struct _Buffer* buf);
void arrangeCursor(struct _Buffer* buf);
void arrangeLine(struct _Buffer* buf);
void cursorXY(struct _Buffer* buf, int x, int y);
void restorePosition(struct _Buffer* buf, struct _Buffer* orig);
