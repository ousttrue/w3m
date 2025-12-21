#pragma once
#include <stdbool.h>

struct TermcapEntry {
    char bp[1024];
    char funcstr[256];
    char gcmap[96];

    // clear to the end of display
    char* _cd;
    // clear to the end of line
    char* _ce;
    // cursor right
    char* _kr;
    // cursor left
    char* _kl;
    // carriage return
    char* _cr;
    char* _bt;
    // tab
    char* _ta;
    // save cursor
    char* _sc;
    // restore cursor
    char* _rc;
    // standout mode
    char* _so;
    // standout mode end
    char* _se;
    // underline mode
    char* _us;
    // underline mode end
    char* _ue;
    // clear screen
    char* _cl;
    // cursor move
    char* _cm;
    // append line
    char* _al;
    // scroll reverse
    char* _sr;
    // bold mode
    char* _md;
    // bold mode end
    char* _me;
    // terminal init
    char* _ti;
    // terminal end
    char* _te;
    // move right one space
    char* _nd;
    // alternative (graphic) charset start
    char* _as;
    // alternative (graphic) charset end
    char* _ae;
    // enable alternative charset
    char* _eA;
    // graphics charset pairs
    char* _ac;
    // set default color pair to its original value
    char* _op;
};

bool termcap_read(struct TermcapEntry* te, const char* ent);

struct TermPosition {
    int line;
    int column;
};

const char* termcap_str_move(struct TermcapEntry* te, struct TermPosition position);
