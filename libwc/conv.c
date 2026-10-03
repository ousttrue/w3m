#include "conv.h"
#include "status.h"
#include "detect.h"
#include "wtf.h"
#include "iso2022.h"
#include "hz.h"
#include "ucs.h"
#include "utf8.h"
#include "utf7.h"
#include <assert.h>

const uint8_t* WcReplace = (const uint8_t*)"?";
const uint8_t* WcReplaceW = (const uint8_t*)"??";

static void
wc_conv_to_ces(struct wc_option* WcOption, struct Writer* w, const uint8_t* sp, const uint8_t* ep, wc_ces ces)
{
    const uint8_t* p;
    switch (ces) {
    case WC_CES_HZ_GB_2312:
        for (p = sp; p < ep && *p != '~' && *p < 0x80; p++)
            ;
        break;
    case WC_CES_TCVN_5712:
    case WC_CES_VISCII_11:
    case WC_CES_VPS:
        for (p = sp; p < ep && 0x20 <= *p && *p < 0x80; p++)
            ;
        break;
    default:
        for (p = sp; p < ep && *p < 0x80; p++)
            ;
        break;
    }
    if (p == ep) {
        CALL2(w, setBeginEnd, sp, ep);
        return;
    }

    if (p > sp)
        p--; /* for precompose */
    if (p > sp)
        CALL2(w, pushStrLen, sp, (int)(p - sp));

    struct wc_status st;
    wc_output_init(WcOption, ces, &st);

    switch (ces) {
    case WC_CES_ISO_2022_JP:
    case WC_CES_ISO_2022_JP_2:
    case WC_CES_ISO_2022_JP_3:
    case WC_CES_ISO_2022_CN:
    case WC_CES_ISO_2022_KR:
    case WC_CES_HZ_GB_2312:
    case WC_CES_TCVN_5712:
    case WC_CES_VISCII_11:
    case WC_CES_VPS:
    case WC_CES_UTF_8:
    case WC_CES_UTF_7:
        while (p < ep)
            (*st.ces_info->push_to)(WcOption, w, wtf_parse(WcOption, &p), &st);
        break;
    default:
        while (p < ep) {
            if (*p < 0x80 && wtf_width(WcOption, p + 1)) {
                CALL1(w, pushChar, *p);
                p++;
            } else
                (*st.ces_info->push_to)(WcOption, w, wtf_parse(WcOption, &p), &st);
        }
        break;
    }

    wc_push_end(WcOption, w, &st);
}

void wc_Str_conv(struct wc_option* WcOption, struct Writer* w, const uint8_t* sp, const uint8_t* ep, wc_ces f_ces, wc_ces t_ces)
{

    if (f_ces == WC_CES_WTF) {
        if (t_ces == WC_CES_WTF) {
            // nop
            CALL2(w, setBeginEnd, sp, ep);
        } else {
            // wtf => t_ces
            wc_conv_to_ces(WcOption, w, sp, ep, t_ces);
        }
    } else {
        if (t_ces == WC_CES_WTF) {
            // f_ces => wtf
            (*WcCesInfo[WC_CES_INDEX(f_ces)].conv_from)(WcOption, w, sp, ep, f_ces);
        } else {
            // f_ces => wtf => t_ces
            struct Writer tmp = w->newWriter(w); // Strnew_size(ep - sp);
            (*WcCesInfo[WC_CES_INDEX(f_ces)].conv_from)(WcOption, &tmp, sp, ep, f_ces);
            wc_conv_to_ces(WcOption, w, CALL0(&tmp, begin), CALL0(&tmp, end), t_ces);
        }
    }
}

void wc_Str_conv_strict(struct wc_option* _WcOption, struct Writer* w, const uint8_t* sp, const uint8_t* ep, wc_ces f_ces, wc_ces t_ces)
{
    struct wc_option WcOption = *_WcOption;
    WcOption.strict_iso2022 = true;
    WcOption.no_replace = true;
    WcOption.fix_width_conv = false;
    wc_Str_conv(&WcOption, w, sp, ep, f_ces, t_ces);
}

void wc_Str_conv_with_detect(struct wc_option* WcOption, struct Writer* w, const uint8_t* sp, const uint8_t* ep, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    wc_ces detect;
    if (*f_ces == WC_CES_WTF || hint == WC_CES_WTF) {
        *f_ces = WC_CES_WTF;
        detect = WC_CES_WTF;
    } else if (WcOption->auto_detect == WC_OPT_DETECT_OFF) {
        *f_ces = hint;
        detect = hint;
    } else {
        if (*f_ces & WC_CES_T_8BIT)
            hint = *f_ces;
        detect = wc_auto_detect(WcOption, (const char*)sp, ep - sp, hint);
        if (WcOption->auto_detect == WC_OPT_DETECT_ON) {
            if ((detect & WC_CES_T_8BIT) || ((detect & WC_CES_T_NASCII) && !(*f_ces & WC_CES_T_8BIT)))
                *f_ces = detect;
        } else {
            if ((detect & WC_CES_T_ISO_2022) && !(*f_ces & WC_CES_T_8BIT))
                *f_ces = detect;
        }
    }
    wc_Str_conv(WcOption, w, sp, ep, detect, t_ces);
}

void wc_push_end(struct wc_option* WcOption, struct Writer* w, struct wc_status* st)
{
    if (st->ces_info->id & WC_CES_T_ISO_2022)
        wc_push_to_iso2022_end(WcOption, w, st);
    else if (st->ces_info->id == WC_CES_HZ_GB_2312)
        wc_push_to_hz_end(WcOption, w, st);
    else if (st->ces_info->id == WC_CES_UTF_8)
        wc_push_to_utf8_end(WcOption, w, st);
    else if (st->ces_info->id == WC_CES_UTF_7)
        wc_push_to_utf7_end(WcOption, w, st);
}
