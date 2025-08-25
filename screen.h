#pragma once
#include <stddef.h>
#include "writer.h"

extern int Do_not_use_ti_te;

// screen to tty
void refresh(const struct Writer *writer);

void getTCstr();
void setupscreen();
void move(int line, int column);
void addmch(char* p, size_t len);
void addch(char c);
void wrap();
void touch_line();
void standout();
void standend();
void bold();
void boldend();
void underline();
void underlineend();
void graphstart();
void graphend();
void setfcolor(int color);
void setbcolor(int color);
void clear(const struct Writer *writer);
void clrtoeol();
void clrtoeolx();
void clrtobot();
void clrtobotx();
void no_clrtoeol();
void addstr(char* s);
void addnstr(char* s, int n);
void addnstr_sup(char* s, int n);
void toggle_stand();
void bell(const struct Writer *writer);
void touch_cursor();
