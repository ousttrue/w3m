#include "conv.h"
#include "status.h"
#include "detect.h"
#include "ces.h"
#include "wtf.h"
#include "iso2022.h"
#include "hz.h"
#include "utf8.h"
#include "utf7.h"

char* WcReplace = "?";
char* WcReplaceW = "??";

static Str wc_conv_to_ces(Str is, enum wc_ces ces);

Str wc_Str_conv(Str is, enum wc_ces f_ces, enum wc_ces t_ces)
{
    if (f_ces != WC_CES_WTF) {
        int index = WC_CES_INDEX(f_ces);
        struct wc_ces_info* info = &WcCesInfo[index];
        is = (info->conv_from)(is, f_ces);
    }
    if (t_ces != WC_CES_WTF) {
        return wc_conv_to_ces(is, t_ces);
    }
    return is;
}

Str wc_Str_conv_strict(Str is, enum wc_ces f_ces, enum wc_ces t_ces)
{
    struct wc_option opt = WcOption;
    WcOption.strict_iso2022 = WC_TRUE;
    WcOption.no_replace = WC_TRUE;
    WcOption.fix_width_conv = WC_FALSE;
    Str os = wc_Str_conv(is, f_ces, t_ces);
    WcOption = opt;
    return os;
}

static Str
wc_conv_to_ces(Str is, enum wc_ces ces)
{
    Str os;
    wc_uchar* sp = (wc_uchar*)is->ptr;
    wc_uchar* ep = sp + is->length;
    wc_uchar* p;
    struct wc_status st;

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
    if (p == ep)
        return is;

    os = Strnew_size(is->length);
    if (p > sp)
        p--; /* for precompose */
    if (p > sp)
        Strcat_charp_n(os, is->ptr, (int)(p - sp));

    wc_output_init(&st, ces);

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
            (*st.ces_info->push_to)(os, wtf_parse(&p), &st);
        break;
    default:
        while (p < ep) {
            if (*p < 0x80 && wtf_width((const char*)p + 1)) {
                Strcat_char(os, (char)*p);
                p++;
            } else
                (*st.ces_info->push_to)(os, wtf_parse(&p), &st);
        }
        break;
    }

    wc_push_end(&st, os);

    return os;
}

struct Converted wc_Str_conv_with_detect(Str is, enum wc_ces f_ces, enum wc_ces hint, enum wc_ces t_ces)
{
    struct Converted converted = { 
        .os = 0,
        .detected = WC_CES_US_ASCII,
    };
    if (f_ces == WC_CES_WTF || hint == WC_CES_WTF) {
        f_ces = WC_CES_WTF;
        converted.detected = WC_CES_WTF;
    } else if (WcOption.auto_detect == WC_OPT_DETECT_OFF) {
        f_ces = hint;
        converted.detected = hint;
    } else {
        if (f_ces & WC_CES_T_8BIT)
            hint = f_ces;
        converted.detected = wc_auto_detect(is->ptr, is->length, hint);
        if (WcOption.auto_detect == WC_OPT_DETECT_ON) {
            if ((converted.detected & WC_CES_T_8BIT) //
                || ((converted.detected & WC_CES_T_NASCII) && !(f_ces & WC_CES_T_8BIT)))
                f_ces = converted.detected;
        } else {
            if ((converted.detected & WC_CES_T_ISO_2022) && !(f_ces & WC_CES_T_8BIT))
                f_ces = converted.detected;
        }
    }
    converted.os = wc_Str_conv(is, converted.detected, t_ces);
    return converted;
}

void wc_push_end(struct wc_status* st, Str os)
{
    if (st->ces_info->id & WC_CES_T_ISO_2022)
        wc_push_to_iso2022_end(os, st);
    else if (st->ces_info->id == WC_CES_HZ_GB_2312)
        wc_push_to_hz_end(os, st);
    else if (st->ces_info->id == WC_CES_UTF_8)
        wc_push_to_utf8_end(os, st);
    else if (st->ces_info->id == WC_CES_UTF_7)
        wc_push_to_utf7_end(os, st);
}
