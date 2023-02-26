#pragma once

struct TermcapEntry
{
    char funcstr[256];

    const char *T_cd = nullptr;
    const char *T_ce = nullptr;
    const char *T_kr = nullptr;
    const char *T_kl = nullptr;
    const char *T_cr = nullptr;
    const char *T_bt = nullptr;
    const char *T_ta = nullptr;
    const char *T_sc = nullptr;
    const char *T_rc = nullptr;
    const char *T_so = nullptr;
    const char *T_se = nullptr;
    const char *T_us = nullptr;
    const char *T_ue = nullptr;
    const char *T_cl = nullptr;

private:
    //! move
    const char *T_cm = nullptr;

public:
    const char *T_al = nullptr;
    const char *T_sr = nullptr;
    const char *T_md = nullptr;
    const char *T_me = nullptr;
    const char *T_ti = nullptr;
    const char *T_te = nullptr;
    const char *T_nd = nullptr;
    const char *T_as = nullptr;
    const char *T_ae = nullptr;
    const char *T_eA = nullptr;
    const char *T_ac = nullptr;
    const char *T_op = nullptr;

    void load(const char *ent);

    char gcmap[96];
    void setgraphchar(void);

    char graphchar(char c)
    {
        return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c));
    }

    const char *move(int line, int column);
};
