#include "term_entry.h"
#include <gcstr/gcstr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>

enum GraphicCharType UseGraphicChar =(GRAPHIC_CHAR_CHARSET);

struct TermEntry T_;

static void
setgraphchar()
{
    for (int c = 0; c < 96; c++)
        T_.gcmap[c] = (char)(c + ' ');

    if (!T_.ac)
        return;

    int n = strlen(T_.ac);
    for (int i = 0; i < n - 1; i += 2) {
        int c = (unsigned)T_.ac[i] - ' ';
        if (c >= 0 && c < 96)
            T_.gcmap[c] = T_.ac[i + 1];
    }
}

#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

void getTCstr()
{
    char* ent = getenv("TERM");
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        // reset_error_exit(0);
        exit(-1);
    }

    int r = tgetent(T_.bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        // reset_error_exit(0);
        exit(-1);
    }

    // for GETSTR macro
    char* suc;
    char* pt = T_.funcstr;

    GETSTR(T_.ce, "ce"); /* clear to the end of line */
    GETSTR(T_.cd, "cd"); /* clear to the end of display */
    GETSTR(T_.kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(T_.kr, "kr");
    if (tgetflag("bs"))
        T_.kl = "\b"; /* cursor left */
    else {
        GETSTR(T_.kl, "le");
        if (suc == NULL)
            GETSTR(T_.kl, "kb");
        if (suc == NULL)
            GETSTR(T_.kl, "kl");
    }
    GETSTR(T_.cr, "cr"); /* carriage return */
    GETSTR(T_.ta, "ta"); /* tab */
    GETSTR(T_.sc, "sc"); /* save cursor */
    GETSTR(T_.rc, "rc"); /* restore cursor */
    GETSTR(T_.so, "so"); /* standout mode */
    GETSTR(T_.se, "se"); /* standout mode end */
    GETSTR(T_.us, "us"); /* underline mode */
    GETSTR(T_.ue, "ue"); /* underline mode end */
    GETSTR(T_.md, "md"); /* bold mode */
    GETSTR(T_.me, "me"); /* bold mode end */
    GETSTR(T_.cl, "cl"); /* clear screen */
    GETSTR(T_.cm, "cm"); /* cursor move */
    GETSTR(T_.al, "al"); /* append line */
    GETSTR(T_.sr, "sr"); /* scroll reverse */
    GETSTR(T_.ti, "ti"); /* terminal init */
    GETSTR(T_.te, "te"); /* terminal end */
    GETSTR(T_.nd, "nd"); /* move right one space */
    GETSTR(T_.eA, "eA"); /* enable alternative charset */
    GETSTR(T_.as, "as"); /* alternative (graphic) charset start */
    GETSTR(T_.ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(T_.ac, "ac"); /* graphics charset pairs */
    GETSTR(T_.op, "op"); /* set default color pair to its original value */

    setgraphchar();
}

bool graph_ok()
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return T_.as[0] != 0 && T_.ae[0] != 0 && T_.ac[0] != 0;
}

