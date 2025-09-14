#include "TermEntry.h"
#include <stdio.h>
#include <stdlib.h> // getenv
#include <string.h>
#ifndef _WIN32
#include <term.h>
#endif

struct TermEntry T = { 0 };
struct TermEntry* getTermEntry()
{
    return &T;
}

struct TermEntry* initTerm()
{
#ifdef _WIN32
#else
    const char* ent = getenv("TERM");
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        // reset_error_exit(SIGNAL_ARGLIST);
        abort();
    }

    char bp[1024];
    int r = tgetent(bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        // reset_error_exit(SIGNAL_ARGLIST);
        abort();
    }

    char* pt = T.funcstr;
    T.ce = tgetstr("ce", &pt); /* clear to the end of line */
    T.cd = tgetstr("cd", &pt); /* clear to the end of display */
    T.kr = tgetstr("nd", &pt); /* cursor right */
    if (T.kr == NULL)
        T.kr = tgetstr("kr", &pt);
    if (tgetflag("bs"))
        T.kl = "\b"; /* cursor left */
    else {
        T.kl = tgetstr("le", &pt);
        if (T.kl == NULL)
            T.kl = tgetstr("kb", &pt);
        if (T.kl == NULL)
            T.kl = tgetstr("kl", &pt);
    }
    T.cr = tgetstr("cr", &pt); /* carriage return */
    T.ta = tgetstr("ta", &pt); /* tab */
    T.sc = tgetstr("sc", &pt); /* save cursor */
    T.rc = tgetstr("rc", &pt); /* restore cursor */
    T.so = tgetstr("so", &pt); /* standout mode */
    T.se = tgetstr("se", &pt); /* standout mode end */
    T.us = tgetstr("us", &pt); /* underline mode */
    T.ue = tgetstr("ue", &pt); /* underline mode end */
    T.md = tgetstr("md", &pt); /* bold mode */
    T.me = tgetstr("me", &pt); /* bold mode end */
    T.cl = tgetstr("cl", &pt); /* clear screen */
    T.cm = tgetstr("cm", &pt); /* cursor move */
    T.al = tgetstr("al", &pt); /* append line */
    T.sr = tgetstr("sr", &pt); /* scroll reverse */
    T.ti = tgetstr("ti", &pt); /* terminal init */
    T.te = tgetstr("te", &pt); /* terminal end */
    T.nd = tgetstr("nd", &pt); /* move right one space */
    T.eA = tgetstr("eA", &pt); /* enable alternative charset */
    T.as = tgetstr("as", &pt); /* alternative (graphic) charset start */
    T.ae = tgetstr("ae", &pt); /* alternative (graphic) charset end */
    T.ac = tgetstr("ac", &pt); /* graphics charset pairs */
    T.op = tgetstr("op", &pt); /* set default color pair to its original value */
#endif

    return &T;
}

const char* getMoveXY(int x, int y)
{
#ifdef _WIN32
    static char buf[32];
    return buf;
#else
    const char *p =  tgoto(T.cm, x, y);
    return p;
#endif
}
