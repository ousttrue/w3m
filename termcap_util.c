#include "termcap_util.h"
#include <stdio.h>
#include <string.h>
#include <termcap.h>

static void
setgraphchar(struct TermcapEntry* te)
{
    for (int c = 0; c < 96; c++) {
        te->gcmap[c] = (char)(c + ' ');
    }

    if (te->_ac) {
        return;
    }
    size_t n = strlen(te->_ac);
    for (int i = 0; i < n - 1; i += 2) {
        int c = (unsigned char)te->_ac[i] - ' ';
        if (c >= 0 && c < 96) {
            te->gcmap[c] = te->_ac[i + 1];
        }
    }
}

bool termcap_read(struct TermcapEntry* te, const char* ent)
{
    int r = tgetent(te->bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        return false;
    }

    char* pt = te->funcstr;
    te->_cd = tgetstr("cd", &pt);
    te->_ce = tgetstr("ce", &pt);

    te->_kr = tgetstr("nd", &pt);
    if (!te->_kr)
        te->_kr = tgetstr("kr", &pt);

    if (tgetflag("bs")) {
        te->_kl = "\b";
    } else {
        te->_kl = tgetstr("le", &pt);
        if (!te->_kl)
            te->_kl = tgetstr("kb", &pt);
        if (!te->_kl)
            te->_kl = tgetstr("kl", &pt);
    }

    te->_cr = tgetstr("cr", &pt);
    te->_ta = tgetstr("ta", &pt);
    te->_sc = tgetstr("sc", &pt);
    te->_rc = tgetstr("rc", &pt);
    te->_so = tgetstr("so", &pt);
    te->_se = tgetstr("se", &pt);
    te->_us = tgetstr("us", &pt);
    te->_ue = tgetstr("ue", &pt);
    te->_md = tgetstr("md", &pt);
    te->_me = tgetstr("me", &pt);
    te->_cl = tgetstr("cl", &pt);
    te->_cm = tgetstr("cm", &pt);
    te->_al = tgetstr("al", &pt);
    te->_sr = tgetstr("sr", &pt);
    te->_ti = tgetstr("ti", &pt);
    te->_te = tgetstr("te", &pt);
    te->_nd = tgetstr("nd", &pt);
    te->_eA = tgetstr("eA", &pt);
    te->_as = tgetstr("as", &pt);
    te->_ae = tgetstr("ae", &pt);
    te->_ac = tgetstr("ac", &pt);
    te->_op = tgetstr("op", &pt);

    setgraphchar(te);

    return true;
}

const char* termcap_str_move(struct TermcapEntry* te, struct TermPosition position)
{
    return tgoto(te->_cm, position.column, position.line);
}
