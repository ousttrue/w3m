#include "option.h"
#include "status.h"
#include "big5.h"
#include "wtf.h"
#include "ucs.h"

#define C0 WC_BIG5_MAP_C0
#define GL WC_BIG5_MAP_GL
#define C1 WC_BIG5_MAP_C1
#define LB WC_BIG5_MAP_LB
#define UB WC_BIG5_MAP_UB

uint8_t WC_BIG5_MAP[ 0x100 ] = {
    C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0,
    C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0, C0,
    GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL,
    GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL, GL,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB,
    LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, LB, C0,

    C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1,
    C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1, C1,
    C1, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB,
    UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, UB, C1,
};

wc_wchar_t
wc_big5_to_cs94w(wc_wchar_t cc)
{
    cc.code = WC_BIG5_N(cc.code);
    if (cc.code < WC_C_BIG5_2_BASE)
        cc.ccs = WC_CCS_BIG5_1;
    else
    {
        cc.ccs = WC_CCS_BIG5_2;
        cc.code -= WC_C_BIG5_2_BASE;
    }
    cc.code = WC_N_CS94W(cc.code);
    return cc;
}

wc_wchar_t
wc_cs94w_to_big5(wc_wchar_t cc)
{
    cc.code = WC_CS94W_N(cc.code);
    if (cc.ccs == WC_CCS_BIG5_2)
        cc.code += WC_C_BIG5_2_BASE;
    cc.code = WC_N_BIG5(cc.code);
    cc.ccs = WC_CCS_BIG5;
    return cc;
}

Str wc_conv_from_big5(Str is, CharacterEncodingScheme ces)
{
    auto sp = is->data();
    auto ep = sp + is->Size();

    const uint8_t *p;
    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep)
        return is;
    auto os = Strnew_size(is->Size());
    if (p > sp)
        os->Push(is->c_str(), (int)(p - sp));

    int state = WC_BIG5_NOSTATE;
    for (; p < ep; p++)
    {
        switch (state)
        {
        case WC_BIG5_NOSTATE:
            switch (WC_BIG5_MAP[*p])
            {
            case UB:
                state = WC_BIG5_MBYTE1;
                break;
            case C1:
                wtf_push_unknown(os, p, 1);
                break;
            default:
                os->Push((char)*p);
                break;
            }
            break;
        case WC_BIG5_MBYTE1:
            if (WC_BIG5_MAP[*p] & LB)
                wtf_push(os, WC_CCS_BIG5, ((uint32_t) * (p - 1) << 8) | *p);
            else
                wtf_push_unknown(os, p - 1, 2);
            state = WC_BIG5_NOSTATE;
            break;
        }
    }
    switch (state)
    {
    case WC_BIG5_MBYTE1:
        wtf_push_unknown(os, p - 1, 1);
        break;
    }
    return os;
}

void wc_push_to_big5(Str os, wc_wchar_t cc, wc_status *st)
{
    while (1)
    {
        switch (cc.ccs)
        {
        case WC_CCS_US_ASCII:
            os->Push((char)cc.code);
            return;
        case WC_CCS_BIG5_1:
        case WC_CCS_BIG5_2:
            cc = wc_cs94w_to_big5(cc);
        case WC_CCS_BIG5:
            os->Push((char)(cc.code >> 8));
            os->Push((char)(cc.code & 0xff));
            return;
        case WC_CCS_UNKNOWN_W:
            if (!WcOption.no_replace)
                os->Push(WC_REPLACE_W);
            return;
        case WC_CCS_UNKNOWN:
            if (!WcOption.no_replace)
                os->Push(WC_REPLACE);
            return;
        default:

            if (WcOption.ucs_conv)
                cc = wc_any_to_any_ces(cc, st);
            else

                cc.ccs = WC_CCS_IS_WIDE(cc.ccs) ? WC_CCS_UNKNOWN_W : WC_CCS_UNKNOWN;
            continue;
        }
    }
}

Str wc_char_conv_from_big5(uint8_t c, wc_status *st)
{
    static Str os;
    static uint8_t big5u;

    if (st->state == -1)
    {
        st->state = WC_BIG5_NOSTATE;
        os = Strnew_size(8);
    }

    switch (st->state)
    {
    case WC_BIG5_NOSTATE:
        switch (WC_BIG5_MAP[c])
        {
        case UB:
            big5u = c;
            st->state = WC_BIG5_MBYTE1;
            return NULL;
        case C1:
            break;
        default:
            os->Push((char)c);
            break;
        }
        break;
    case WC_BIG5_MBYTE1:
        if (WC_BIG5_MAP[c] & LB)
            wtf_push(os, WC_CCS_BIG5, ((uint32_t)big5u << 8) | c);
        break;
    }
    st->state = -1;
    return os;
}
