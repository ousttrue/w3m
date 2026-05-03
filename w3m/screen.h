#pragma once
#include <stddef.h>
#include <stdint.h>

struct Usize2 {
    size_t x;
    size_t y;
};

enum AnsiColor {
    ANSI_COLOR,
    ANSI_BLACK,
    ANSI_RED,
    ANSI_GREEN,
    ANSI_YELLOW,
    ANSI_BLUE,
    ANSI_MAGENTA,
    ANSI_CYAN,
    ANSI_WHITE,
    ANSI_TERM,
};

void sc_init(struct Usize2 size);

void sc_move(size_t line, size_t column);
void sc_addmch(const uint8_t* p, size_t len);
void sc_addch(uint8_t c);
void sc_toggle_stand(void);
void sc_standout(void);
void sc_standend(void);
void sc_bold(void);
void sc_boldend(void);
void sc_underline(void);
void sc_underlineend(void);
void sc_graphstart(void);
void sc_graphend(void);
void sc_setfcolor(enum AnsiColor color);
void sc_setbcolor(enum AnsiColor color);
void sc_clear(void);
void sc_clrtoeolx(void);
void sc_clrtobotx(void);
void sc_addstr(const char* s);
void sc_addnstr(const char* s, size_t n);
void sc_addnstr_sup(const char* s, size_t n);

static inline void sc_mvaddnstr(int y, int x, const char* str, int n)
{
    sc_move(y, x);
    sc_addnstr_sup(str, n);
}

static inline void sc_mvaddch(int y, int x, int c)
{
    sc_move(y, x);
    sc_addch(c);
}

static inline void sc_mvaddstr(int y, int x, const char* str)
{
    sc_move(y, x);
    sc_addstr(str);
}
