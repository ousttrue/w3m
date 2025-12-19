#pragma once
#include <stddef.h>
#include <stdint.h>

void screen_setup(int lines, int cols);
void screen_move(int line, int column);
void screen_addmch(char* p, size_t len);
inline static void addch(char c)
{
    screen_addmch(&c, 1);
}
void screen_wrap(void);
void screen_touch_line(void);
void screen_standout(void);
void screen_standend(void);
void screen_toggle_stand(void);
void screen_bold(void);
void screen_boldend(void);
void screen_underline(void);
void screen_underlineend(void);
void screen_graphstart(void);
void screen_graphend(void);
void screen_setfcolor(int color);
void screen_setbcolor(int color);
void refresh(void);
void screen_clear(void);
void screen_clrtoeol(void);
void screen_clrtoeolx(void);
void screen_clrtobot(void);
void screen_clrtobotx(void);
void screen_addstr(char* s);
void screen_addnstr(char* s, int n);
void screen_addnstr_sup(char* s, int n);
void screen_touch_cursor(void);
void screen_touch_column(int col);
