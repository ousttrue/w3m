#pragma once
#include <stddef.h>
#include <stdint.h>

void setupscreen(int lines, int cols);
void move(int line, int column);
void addmch(char* p, size_t len);
inline static void addch(char c)
{
    addmch(&c, 1);
}
void wrap(void);
void touch_line(void);
void standout(void);
void standend(void);
void toggle_stand(void);
void bold(void);
void boldend(void);
void underline(void);
void underlineend(void);
void graphstart(void);
void graphend(void);
void setfcolor(int color);
void setbcolor(int color);
void refresh(void);
void clear(void);
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void addstr(char* s);
void addnstr(char* s, int n);
void addnstr_sup(char* s, int n);
void touch_cursor(void);
