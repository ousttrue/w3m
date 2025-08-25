#pragma once
#include <stddef.h>

// screen to tty
extern void refresh(void);

extern int LINES, COLS;
void setlinescols(void);
extern void getTCstr(void);
extern void setlinescols(void);
extern void setupscreen(void);
extern int initscr(void);
extern void move(int line, int column);
extern void addmch(char* p, size_t len);
extern void addch(char c);
extern void wrap(void);
extern void touch_line(void);
extern void standout(void);
extern void standend(void);
extern void bold(void);
extern void boldend(void);
extern void underline(void);
extern void underlineend(void);
extern void graphstart(void);
extern void graphend(void);
extern void setfcolor(int color);
extern void setbcolor(int color);
extern void clear(void);
extern void clrtoeol(void);
extern void clrtoeolx(void);
extern void clrtobot(void);
extern void clrtobotx(void);
extern void no_clrtoeol(void);
extern void addstr(char* s);
extern void addnstr(char* s, int n);
extern void addnstr_sup(char* s, int n);
extern void toggle_stand(void);
extern void bell(void);
extern void touch_cursor(void);
