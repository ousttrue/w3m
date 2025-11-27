#pragma once
#include <stdbool.h>

enum GraphicCharType {
    GRAPHIC_CHAR_CHARSET = 0,
    GRAPHIC_CHAR_DEC = 1,
    GRAPHIC_CHAR_ASCII = 2,
};

extern enum GraphicCharType UseGraphicChar;

struct TermEntry {
    char bp[1024], funcstr[256];
    char gcmap[96];

    /// clear to the end of display
    char* cd;
    /// clear to the end of line
    char* ce;
    /// cursor a right
    char* kr;
    /// cursor a left
    char* kl;
    /// carriage return (^M)
    char* cr;
    /// back tab
    char* bt;
    /// tab (^I)
    char* ta;
    /// save cursor
    char* sc;
    /// restore cursor
    char* rc;
    /// standout start
    char* so;
    /// standout end
    char* se;
    /// underline start
    char* us;
    /// underline end
    char* ue;
    /// clear screen
    char* cl;
    /// cursor move
    char* cm;
    /// append line
    char* al;
    /// scroll reverse
    char* sr;
    /// start bold
    char* md;
    /// end all attributes
    char* me;
    /// terminal init
    char* ti;
    /// terminal end
    char* te;
    /// move right one space
    char* nd;
    /// alternative (graphic) charset start
    char* as;
    /// alternative (graphic) charset end
    char* ae;
    /// enable alternative charset
    char* eA;
    /// graphics charset pairs
    char* ac;
    /// set default color pair to its original value
    char* op;
};
extern struct TermEntry T_;

void getTCstr();
bool graph_ok();
