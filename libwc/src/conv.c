#include "conv.h"
#include "detect.h"
#include "status.h"
#include "ces.h"
#include "wtf.h"
#include "iso2022.h"
#include "hz.h"
#include "ucs.h"
#include "utf8.h"
#include "utf7.h"

char* WcReplace = "?";
char* WcReplaceW = "??";

static struct wc_output
wc_conv_to_ces(struct wc_option opts, const char* is, int len, wc_ces ces)
{
    wc_uchar* sp = (wc_uchar*)is;
    wc_uchar* ep = sp + len;

    wc_uchar* p;
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
        return wc_output_from_charp(is);

    struct wc_output os = wc_output_from_size(len);
    if (p > sp)
        p--; /* for precompose */
    if (p > sp)
        wc_output__cat_charp_n(&os, is, (int)(p - sp));

    struct wc_status st;
    wc_output_init(opts, ces, &st);

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
            (*st.ces_info->push_to)(opts, &os, wtf_parse(opts, &p), &st);
        break;
    default:
        while (p < ep) {
            if (*p < 0x80 && wtf_width(opts, p[1])) {
                wc_output__cat_char(&os, (char)*p);
                p++;
            } else
                (*st.ces_info->push_to)(opts, &os, wtf_parse(opts, &p), &st);
        }
        break;
    }

    wc_push_end(&os, &st);

    return os;
}

struct wc_output wc_Str_conv(struct wc_option opts, const char* _is, int len, wc_ces f_ces, wc_ces t_ces)
{
    struct wc_output is = (f_ces != WC_CES_WTF)
        ? (*WcCesInfo[WC_CES_INDEX(f_ces)].conv_from)(opts, _is, len, f_ces)
        : wc_output_from_charp_n(_is, len);

    if (t_ces != WC_CES_WTF)
        return wc_conv_to_ces(opts, wc_output__ptr(is), wc_output__len(is), t_ces);
    else
        return is;
}

struct wc_output wc_Str_conv_strict(struct wc_option opts, const char* is, int len, wc_ces f_ces, wc_ces t_ces)
{
    opts.strict_iso2022 = WC_TRUE;
    opts.no_replace = WC_TRUE;
    opts.fix_width_conv = WC_FALSE;
    struct wc_output os = wc_Str_conv(opts, is, len, f_ces, t_ces);
    return os;
}

struct wc_output wc_Str_conv_with_detect(struct wc_option opts, const uint8_t* is, int len, wc_ces* f_ces, wc_ces hint, wc_ces t_ces)
{
    wc_ces detect;
    if (*f_ces == WC_CES_WTF || hint == WC_CES_WTF) {
        *f_ces = WC_CES_WTF;
        detect = WC_CES_WTF;
    } else if (opts.auto_detect == WC_OPT_DETECT_OFF) {
        *f_ces = hint;
        detect = hint;
    } else {
        if (*f_ces & WC_CES_T_8BIT)
            hint = *f_ces;
        detect = wc_auto_detect(opts, is, len, hint);
        if (opts.auto_detect == WC_OPT_DETECT_ON) {
            if ((detect & WC_CES_T_8BIT) || ((detect & WC_CES_T_NASCII) && !(*f_ces & WC_CES_T_8BIT)))
                *f_ces = detect;
        } else {
            if ((detect & WC_CES_T_ISO_2022) && !(*f_ces & WC_CES_T_8BIT))
                *f_ces = detect;
        }
    }
    return wc_Str_conv(opts, is, len, detect, t_ces);
}

static wc_ces char_conv_f_ces = 0, char_conv_t_ces = WC_CES_WTF;
static struct wc_status char_conv_st;

void wc_char_conv_init(wc_ces f_ces, wc_ces t_ces)
{
    wc_input_init(f_ces, &char_conv_st);
    char_conv_st.state = -1;
    char_conv_f_ces = f_ces;
    char_conv_t_ces = t_ces;
}

struct wc_output wc_char_conv(struct wc_option opts, char c)
{
    struct wc_output is = (*char_conv_st.ces_info->char_conv)(opts, (wc_uchar)c, &char_conv_st);
    return wc_Str_conv(opts, wc_output__ptr(is), wc_output__len(is), WC_CES_WTF, char_conv_t_ces);
}
