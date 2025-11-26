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
    char *cd, *ce, *kr, *kl, *cr, *bt, *ta, *sc, *rc,
        *so, *se, *us, *ue, *cl, *cm, *al, *sr, *md, *me,
        *ti, *te, *nd, *as, *ae, *eA, *ac, *op;
    char gcmap[96];
};
extern struct TermEntry T_;

void getTCstr();
bool graph_ok();
