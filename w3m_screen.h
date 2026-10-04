#pragma once
#include <stddef.h>
#include <stdio.h>

#define DEFAULT_COLS 80

void setupscreen(void);

// render to tty
void refresh(FILE* ttyf);

void clear(void);
void move(int line, int column);
void addmch(const char* p, size_t len);
void addch(char c);
void standout(void);
void standend(void);
void bold(void);
void boldend(void);
void underline(void);
void underlineend(void);
void graphstart(void);
void graphend(void);
int graph_ok(void);
void setfcolor(int color);
void setbcolor(int color);
void clrtoeolx(void);
void clrtobotx(void);
void addstr(const char* s);
void addnstr(const char* s, int n);
void addnstr_sup(const char* s, int n);
void toggle_stand(void);
void touch_cursor(void);
void touch_line(void);
void touch_column(int col);
