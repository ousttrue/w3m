#include "termcap_entry.h"
#include <string.h>
#include <stdexcept>

// -lncurses
extern "C"
{
    extern int tgetent(char *, const char *);
    extern int tgetflag(const char *);
    extern char *tgetstr(const char *, char **);
    extern char *tgoto(const char *, int, int);
}

class EntryGetter
{
    char *pt = nullptr;

public:
    EntryGetter(char *buf)
        : pt(buf)
    {
    }

    char *GETSTR(const char *s)
    {
        return tgetstr(s, &pt);
    }
};

void TermcapEntry::load(const char *ent)
{
    if (!ent)
    {
        // fprintf(stderr, "TERM is not set\n");
        // reset_error_exit(SIGNAL_ARGLIST);
        throw std::runtime_error("TERM is not set");
    }

    // char *suc;
    // int r;
    char bp[1024];
    auto r = tgetent(bp, ent);
    if (r != 1)
    {
        /* Can't find termcap entry */
        // fprintf(stderr, "Can't find termcap entry %s\n", ent);
        // reset_error_exit(SIGNAL_ARGLIST);
        throw std::runtime_error(std::string("Can't find termcap entry {}") + ent);
    }

    EntryGetter getter(funcstr);

    T_ce = getter.GETSTR("ce"); /* clear to the end of line */
    T_cd = getter.GETSTR("cd"); /* clear to the end of display */
    T_kr = getter.GETSTR("nd"); /* cursor right */
    if (!T_kr)
    {
        T_kr = getter.GETSTR("kr");
    }
    if (tgetflag("bs"))
    {
        T_kl = "\b"; /* cursor left */
    }
    else
    {
        T_kl = getter.GETSTR("le");
        if (!T_kl)
        {
            T_kl = getter.GETSTR("kb");
        }
        if (!T_kl)
        {
            T_kl = getter.GETSTR("kl");
        }
    }

    T_cr = getter.GETSTR("cr"); /* carriage return */
    T_ta = getter.GETSTR("ta"); /* tab */
    T_sc = getter.GETSTR("sc"); /* save cursor */
    T_rc = getter.GETSTR("rc"); /* restore cursor */
    T_so = getter.GETSTR("so"); /* standout mode */
    T_se = getter.GETSTR("se"); /* standout mode end */
    T_us = getter.GETSTR("us"); /* underline mode */
    T_ue = getter.GETSTR("ue"); /* underline mode end */
    T_md = getter.GETSTR("md"); /* bold mode */
    T_me = getter.GETSTR("me"); /* bold mode end */
    T_cl = getter.GETSTR("cl"); /* clear screen */
    T_cm = getter.GETSTR("cm"); /* cursor move */
    T_al = getter.GETSTR("al"); /* append line */
    T_sr = getter.GETSTR("sr"); /* scroll reverse */
    T_ti = getter.GETSTR("ti"); /* terminal init */
    T_te = getter.GETSTR("te"); /* terminal end */
    T_nd = getter.GETSTR("nd"); /* move right one space */
    T_eA = getter.GETSTR("eA"); /* enable alternative charset */
    T_as = getter.GETSTR("as"); /* alternative (graphic) charset start */
    T_ae = getter.GETSTR("ae"); /* alternative (graphic) charset end */
    T_ac = getter.GETSTR("ac"); /* graphics charset pairs */
    T_op = getter.GETSTR("op"); /* set default color pair to its original value */

    setgraphchar();
}

void TermcapEntry::setgraphchar(void)
{
    for (int c = 0; c < 96; c++)
        gcmap[c] = (char)(c + ' ');

    if (!T_ac)
        return;

    auto n = strlen(T_ac);
    for (int i = 0; i < n - 1; i += 2)
    {
        auto c = (unsigned)T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            gcmap[c] = T_ac[i + 1];
    }
}

const char *TermcapEntry::move(int line, int column) const
{
    return tgoto(T_cm, column, line);
}
