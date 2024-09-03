#pragma once


void setupscreen(int LINES, int COLS);
void clear();
void refresh();
void move(int line, int column);
void touch_line();
void touch_column(int);
void wrap();
void clrtoeol(); /* conflicts with curs_clear(3)? */
void clrtoeolx();
void clrtobot();
void clrtobotx();
void addch(char c);
void addstr(char *s);
void addnstr(char *s, int n);
void addnstr_sup(char *s, int n);
void standout();
void standend();
void toggle_stand(void);
void bold();
void boldend();
void underline();
void underlineend();
void graphend();
void graphstart();
