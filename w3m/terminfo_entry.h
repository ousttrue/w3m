#pragma once
#include <stdbool.h>

struct TermInfo {
    char bp[1024];
    char funcstr[256];
    // clear to the end of display
    char* T_cd;
    // clear to the end of line
    char* T_ce;
    // cursor right
    char* T_kr;
    // cursor left
    char* T_kl;
    // carriage return
    char* T_cr;
    char* T_bt;
    // tab
    char* T_ta;
    // save cursor
    char* T_sc;
    // restore cursor
    char* T_rc;
    // standout mode
    char* T_so;
    // standout mode end
    char* T_se;
    // underline mode
    char* T_us;
    // underline mode end
    char* T_ue;
    // clear screen
    char* T_cl;
    // cursor move
    char* T_cm;
    // append line
    char* T_al;
    // scroll reverse
    char* T_sr;
    // bold mode
    char* T_md;
    // bold mode end
    char* T_me;
    // terminal init
    char* T_ti;
    // terminal end
    char* T_te;
    // move right one space
    char* T_nd;
    // alternative (graphic) charset start
    char* T_as;
    // alternative (graphic) charset end
    char* T_ae;
    // enable alternative charset
    char* T_eA;
    // graphics charset pairs
    char* T_ac;
    // set default color pair to its original value
    char* T_op;

    char gcmap[96];
};
void getTCstr(struct TermInfo* ti);

typedef int (*PutC)(int);

extern int tputs(const char* str, int affcnt, int (*putc)(int));
extern char* tgoto(const char* cm, int destcol, int destline);

void writestr(PutC f, const char* s);

void terminfo_reset(PutC f, struct TermInfo* ti, bool do_not_use_ti_te);

void MOVE(PutC f, struct TermInfo* ti, int line, int column);

bool graph_ok(struct TermInfo* ti);
