#pragma once
#include <stddef.h>
#include <stdint.h>

extern int LINES, COLS;

void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(const char* url, int x, int y, int w, int h);
void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);

void mouse_active();
void mouse_inactive();
void mouse_end();
void reset_tty(void);
void set_int(void);
void setupscreen(void);
int initscr(void);
void move(int line, int column);
void addmch(const uint8_t* p, size_t len);
void addch(uint8_t c);
void wrap(void);
void touch_line(void);
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
void refresh(void);
void clear(void);
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(const char* s);
void addnstr(const char* s, int n);
void addnstr_sup(char* s, int n);

void touch_cursor(void);
