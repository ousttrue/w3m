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
void vt_clear(struct VirtualTerm* vt);
void vt_getTCstr(struct VirtualTerm* vt);
void vt_setupscreen(struct VirtualTerm* vt, int rows, int cols);
void vt_move(struct VirtualTerm* vt, int line, int column);
void vt_addmch(struct VirtualTerm* vt, const char* p, size_t len);
void vt_addch(struct VirtualTerm* vt, char c);
void vt_wrap(struct VirtualTerm* vt);
void vt_touch_line(struct VirtualTerm* vt);

void vt_clrtoeol(struct VirtualTerm* vt);
void vt_clrtoeolx(struct VirtualTerm* vt);
void vt_clrtobot(struct VirtualTerm* vt);
void vt_clrtobotx(struct VirtualTerm* vt);
void vt_no_clrtoeol(struct VirtualTerm* vt);
void vt_addstr(struct VirtualTerm* vt, const char* s);
void vt_addnstr(struct VirtualTerm* vt, const char* s, int n);
void vt_addnstr_sup(struct VirtualTerm* vt, const char* s, int n);
void vt_toggle_stand(struct VirtualTerm* vt);
void vt_touch_cursor(struct VirtualTerm* vt);

inline static void vt_mvaddch(struct VirtualTerm* vt, int y, int x, char c)
{
    vt_move(vt, y, x);
    vt_addch(vt, c);
}

inline static void vt_mvaddstr(struct VirtualTerm* vt, int y, int x, const char* str)
{
    vt_move(vt, y, x);
    vt_addstr(vt, str);
}

inline static void vt_mvaddnstr(struct VirtualTerm* vt, int y, int x, const char* str, int n)
{
    vt_move(vt, y, x);
    vt_addnstr_sup(vt, str, n);
}
