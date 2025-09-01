#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "line_prop.h"

extern int Do_not_use_ti_te;

typedef struct scline {
    char** lineimage;
    l_prop* lineprop;
    enum LineStatus isdirty;
    short eol;
} Screen;

struct VirtualTerm {
    int ROWS;
    int COLS;
    int tab_step;
    int CurLine;
    int CurColumn;
    Screen* ScreenElem;
    Screen** ScreenImage;
    l_prop CurrentMode;
    int graph_enabled;
};

struct VirtualTerm* getScreen();
void clear(struct VirtualTerm* vt);
void getTCstr(struct VirtualTerm* vt);
void setupscreen(struct VirtualTerm* vt, int rows, int cols);
void vt_move(struct VirtualTerm* vt, int line, int column);
void addmch(struct VirtualTerm* vt, char* p, size_t len);
void addch(struct VirtualTerm* vt, char c);
void wrap(struct VirtualTerm* vt);
void touch_line(struct VirtualTerm* vt);

void clrtoeol(struct VirtualTerm* vt);
void clrtoeolx(struct VirtualTerm* vt);
void clrtobot(struct VirtualTerm* vt);
void clrtobotx(struct VirtualTerm* vt);
void no_clrtoeol(struct VirtualTerm* vt);
void addstr(struct VirtualTerm* vt, const char* s);
void addnstr(struct VirtualTerm* vt, const char* s, int n);
void addnstr_sup(struct VirtualTerm* vt, const char* s, int n);
void toggle_stand(struct VirtualTerm* vt);
void touch_cursor(struct VirtualTerm* vt);

inline static void vt_mvaddch(struct VirtualTerm* vt, int y, int x, char c)
{
    vt_move(vt, y, x);
    addch(vt, c);
}

inline static void vt_mvaddstr(struct VirtualTerm* vt, int y, int x, const char* str)
{
    vt_move(vt, y, x);
    addstr(vt, str);
}

inline static void vt_mvaddnstr(struct VirtualTerm* vt, int y, int x, const char* str, int n)
{
    vt_move(vt, y, x);
    addnstr_sup(vt, str, n);
}
