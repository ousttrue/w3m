#pragma once

struct TermcapEntry
{
    char funcstr[256];

    //! clear down
    const char *T_cd = nullptr;
    //! clear to eol
    const char *T_ce = nullptr;
    //! right arrow
    const char *T_kr = nullptr;
    //! lefr arrow
    const char *T_kl = nullptr;
    //! carriage retrurn
    const char *T_cr = nullptr;
    //! back tab
    const char *T_bt = nullptr;
    //! tab
    const char *T_ta = nullptr;
    //! save cursor
    const char *T_sc = nullptr;
    //! restore cursor
    const char *T_rc = nullptr;
    //! standout mode start
    const char *T_so = nullptr;
    //! standout mode end
    const char *T_se = nullptr;
    //! underline start
    const char *T_us = nullptr;
    //! underline end
    const char *T_ue = nullptr;
    //! clear and goto 0, 0
    const char *T_cl = nullptr;

private:
    //! cusor move
    const char *T_cm = nullptr;

public:
    //! add line to cursor next
    const char *T_al = nullptr;
    //! scroll reverse a line
    const char *T_sr = nullptr;
    //! bold start
    const char *T_md = nullptr;
    //! bold end
    const char *T_me = nullptr;

private:
    //! cursor move initialize ?
    const char *T_ti = nullptr;
    //! cursor move end ?
    const char *T_te = nullptr;

public:
    //! no delete space (move right)
    const char *T_nd = nullptr;

    //! alternative charset start
    const char *T_as = nullptr;
    //! alternative charset stop
    const char *T_ae = nullptr;
    //! enable alternative charset
    const char *T_eA = nullptr;
    //! alternative charset map
    const char *T_ac = nullptr;

    //! set default color pair to its original value
    const char *T_op = nullptr;

public:
    void load(const char *ent);

    char gcmap[96];
    void setgraphchar(void);

    char graphchar(char c)
    {
        return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c));
    }

    const char *move(int line, int column);
};
