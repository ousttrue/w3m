#pragma once

struct TermEntry {
    char bp[1024], funcstr[256];
    char *cd, *ce, *kr, *kl, *cr, *bt, *ta, *sc, *rc,
        *so, *se, *us, *ue, *cl, *cm, *al, *sr, *md, *me,
        *ti, *te, *nd, *as, *ae, *eA, *ac, *op;
    char gcmap[96];
};

void getTCstr(struct TermEntry* T);
