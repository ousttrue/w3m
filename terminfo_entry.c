#include "terminfo_entry.h"
#include "global.h"
#include "constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DEFAULT_TERM 0 /* XXX */

extern int tgetent(char* bp, const char* name);

extern int tgetnum(char*);
extern int tgetflag(char*);
extern char* tgetstr(char*, char**);

static void setgraphchar(struct TermInfo* ti)
{
    for (int c = 0; c < 96; c++)
        ti->gcmap[c] = (char)(c + ' ');

    if (ti->T_ac) {
        int n = strlen(ti->T_ac);
        for (int i = 0; i < n - 1; i += 2) {
            uint8_t c = (uint8_t)ti->T_ac[i] - ' ';
            if (c >= 0 && c < 96)
                ti->gcmap[c] = ti->T_ac[i + 1];
        }
    }
}

void getTCstr(struct TermInfo* ti)
{
    char* ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        exit(1);
    }

    int r = tgetent(ti->bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        exit(1);
    }

    char* pt = ti->funcstr;
    ti->T_ce = tgetstr("ce", &pt);
    ti->T_cd = tgetstr("cd", &pt);
    ti->T_kr = tgetstr("nd", &pt);
    if (!ti->T_kr)
        ti->T_kr = tgetstr("kr", &pt);
    if (tgetflag("bs"))
        ti->T_kl = "\b";
    else {
        ti->T_kl = tgetstr("le", &pt);
        if (!ti->T_kl)
            ti->T_kl = tgetstr("kb", &pt);
        if (!ti->T_kl)
            ti->T_kl = tgetstr("kl", &pt);
    }
    ti->T_cr = tgetstr("cr", &pt);
    ti->T_ta = tgetstr("ta", &pt);
    ti->T_sc = tgetstr("sc", &pt);
    ti->T_rc = tgetstr("rc", &pt);
    ti->T_so = tgetstr("so", &pt);
    ti->T_se = tgetstr("se", &pt);
    ti->T_us = tgetstr("us", &pt);
    ti->T_ue = tgetstr("ue", &pt);
    ti->T_md = tgetstr("md", &pt);
    ti->T_me = tgetstr("me", &pt);
    ti->T_cl = tgetstr("cl", &pt);
    ti->T_cm = tgetstr("cm", &pt);
    ti->T_al = tgetstr("al", &pt);
    ti->T_sr = tgetstr("sr", &pt);
    ti->T_ti = tgetstr("ti", &pt);
    ti->T_te = tgetstr("te", &pt);
    ti->T_nd = tgetstr("nd", &pt);
    ti->T_eA = tgetstr("eA", &pt);
    ti->T_as = tgetstr("as", &pt);
    ti->T_ae = tgetstr("ae", &pt);
    ti->T_ac = tgetstr("ac", &pt);
    ti->T_op = tgetstr("op", &pt);

    setgraphchar(ti);

    // LINES = COLS = 0;
    // setlinescols();
}
