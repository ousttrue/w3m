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
void move(struct VirtualTerm* vt, int line, int column);
void addmch(struct VirtualTerm* vt, char* p, size_t len);
void addch(struct VirtualTerm* vt, char c);
void wrap(struct VirtualTerm* vt);
void touch_line(struct VirtualTerm* vt);
void standout(struct VirtualTerm* vt);
void standend(struct VirtualTerm* vt);
void bold(struct VirtualTerm* vt);
void boldend(struct VirtualTerm* vt);
void underline(struct VirtualTerm* vt);
void underlineend(struct VirtualTerm* vt);
void graphstart(struct VirtualTerm* vt);
void graphend(struct VirtualTerm* vt);
void setfcolor(struct VirtualTerm* vt, int color);
void setbcolor(struct VirtualTerm* vt, int color);
void clrtoeol(struct VirtualTerm* vt);
void clrtoeolx(struct VirtualTerm* vt);
void clrtobot(struct VirtualTerm* vt);
void clrtobotx(struct VirtualTerm* vt);
void no_clrtoeol(struct VirtualTerm* vt);
void addstr(struct VirtualTerm* vt, const char* s);
void addnstr(struct VirtualTerm* vt, char* s, int n);
void addnstr_sup(struct VirtualTerm* vt, char* s, int n);
void toggle_stand(struct VirtualTerm* vt);
void touch_cursor(struct VirtualTerm* vt);
