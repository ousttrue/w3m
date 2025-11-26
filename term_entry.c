#include "term_entry.h"
#include <gcstr/gcstr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>

#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

static void
setgraphchar(struct TermEntry* T)
{
    for (int c = 0; c < 96; c++)
        T->gcmap[c] = (char)(c + ' ');

    if (!T->ac)
        return;

    int n = strlen(T->ac);
    for (int i = 0; i < n - 1; i += 2) {
        int c = (unsigned)T->ac[i] - ' ';
        if (c >= 0 && c < 96)
            T->gcmap[c] = T->ac[i + 1];
    }
}

void getTCstr(struct TermEntry* T)
{
    char* suc;

    // for GETSTR macro
    char* pt = T->funcstr;

    char* ent = getenv("TERM");
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        // reset_error_exit(0);
        exit(-1);
    }

    int r = tgetent(T->bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        // reset_error_exit(0);
        exit(-1);
    }

    GETSTR(T->ce, "ce"); /* clear to the end of line */
    GETSTR(T->cd, "cd"); /* clear to the end of display */
    GETSTR(T->kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(T->kr, "kr");
    if (tgetflag("bs"))
        T->kl = "\b"; /* cursor left */
    else {
        GETSTR(T->kl, "le");
        if (suc == NULL)
            GETSTR(T->kl, "kb");
        if (suc == NULL)
            GETSTR(T->kl, "kl");
    }
    GETSTR(T->cr, "cr"); /* carriage return */
    GETSTR(T->ta, "ta"); /* tab */
    GETSTR(T->sc, "sc"); /* save cursor */
    GETSTR(T->rc, "rc"); /* restore cursor */
    GETSTR(T->so, "so"); /* standout mode */
    GETSTR(T->se, "se"); /* standout mode end */
    GETSTR(T->us, "us"); /* underline mode */
    GETSTR(T->ue, "ue"); /* underline mode end */
    GETSTR(T->md, "md"); /* bold mode */
    GETSTR(T->me, "me"); /* bold mode end */
    GETSTR(T->cl, "cl"); /* clear screen */
    GETSTR(T->cm, "cm"); /* cursor move */
    GETSTR(T->al, "al"); /* append line */
    GETSTR(T->sr, "sr"); /* scroll reverse */
    GETSTR(T->ti, "ti"); /* terminal init */
    GETSTR(T->te, "te"); /* terminal end */
    GETSTR(T->nd, "nd"); /* move right one space */
    GETSTR(T->eA, "eA"); /* enable alternative charset */
    GETSTR(T->as, "as"); /* alternative (graphic) charset start */
    GETSTR(T->ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(T->ac, "ac"); /* graphics charset pairs */
    GETSTR(T->op, "op"); /* set default color pair to its original value */

    setgraphchar(T);
}
