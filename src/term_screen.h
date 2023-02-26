#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include <functional>

extern int LINES, COLS;
#define LASTLINE (LINES - 1)

struct TermPos
{
    uint16_t row;
    uint16_t col;
};

class TermScreen
{
public:
    TermPos pos() const;
    std::vector<char> render(const class TermcapEntry &termcap);
};

void move(int line, int column);
void addmch(char *p, size_t len);
void addch(char c);
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
void clear(void);
void scroll(int);
void rscroll(int);
void clrtoeol(void);
void clrtoeolx(void);
void clrtobot(void);
void clrtobotx(void);
void no_clrtoeol(void);
void addstr(char *s);
void addnstr(char *s, int n);
void addnstr_sup(char *s, int n);
void touch_column(int);
