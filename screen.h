#pragma once
#include <stddef.h>

extern int Do_not_use_ti_te;
extern int LINES, COLS;

int initscr();
void resetTerm();

// screen to tty
void refresh();

void setlinescols();
void getTCstr();
void setlinescols();
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
void clear();
void clrtoeol();
void clrtoeolx();
void clrtobot();
void clrtobotx();
void no_clrtoeol();
void addstr(char* s);
void addnstr(char* s, int n);
void addnstr_sup(char* s, int n);
void toggle_stand();
void bell();
void touch_cursor();
