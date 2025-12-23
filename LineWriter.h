#pragma once
#include "line.h"

struct LineWriter {
    int ulmode;
    int somode;
    int bomode;
    int anch_mode;
    int emph_mode;
    int imag_mode;
    int form_mode;
    int active_mode;
    int visited_mode;
    int mark_mode;
    int graph_mode;
    Linecolor color_mode;
};
void addChar(struct LineWriter* this, char c, Lineprop mode);
void addMChar(struct LineWriter* this, char* c, Lineprop mode, size_t len);
void addPasswd(struct LineWriter* this, char* p, Lineprop* pr, int len, int offset, int limit);
void addStr(struct LineWriter* this, char* p, Lineprop* pr, int len, int offset, int limit);
void do_color(struct LineWriter* this, Linecolor c);
void beginLine();
void endLine(struct LineWriter* this);
