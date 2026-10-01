/* vi: set sw=4 ts=8 ai sm noet : */

#include "wc.h"
#include "wtf.h"
#include "iso2022.h"
#include "hz.h"
#ifdef USE_UNICODE
#include "ucs.h"
#include "utf8.h"
#include "utf7.h"
#endif

char *WcReplace = "?";
char *WcReplaceW = "??";

static pStr wc_conv_to_ces(pStr is, wc_ces ces);

pStr
wc_Str_conv(pStr is, wc_ces f_ces, wc_ces t_ces)
{
    if (f_ces != WC_CES_WTF)
	is = (*WcCesInfo[WC_CES_INDEX(f_ces)].conv_from)(is, f_ces);
    if (t_ces != WC_CES_WTF)
	return wc_conv_to_ces(is, t_ces);
    else
	return is;
}

pStr
wc_Str_conv_strict(pStr is, wc_ces f_ces, wc_ces t_ces)
{
    pStr os;
    wc_option opt = WcOption;

    WcOption.strict_iso2022 = WC_TRUE;
    WcOption.no_replace = WC_TRUE;
    WcOption.fix_width_conv = WC_FALSE;
    os = wc_Str_conv(is, f_ces, t_ces);
    WcOption = opt;
    return os;
}

static pStr
wc_conv_to_ces(pStr is, wc_ces ces)
{
    pStr os;
    const wc_uchar *sp = (const wc_uchar *)is->ptr;
    const wc_uchar *ep = sp + is->length;
    const wc_uchar *p;
    wc_status st;

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
	p--;	/* for precompose */
    if (p > sp)
	Strcat_charp_n(os, is->ptr, (int)(p - sp));

    wc_output_init(ces, &st);

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
#ifdef USE_UNICODE
    case WC_CES_UTF_8:
    case WC_CES_UTF_7:
#endif
	while (p < ep)
	    (*st.ces_info->push_to)(os, wtf_parse(&p), &st);
	break;
    default:
	while (p < ep) {
	    if (*p < 0x80 && wtf_width(p + 1)) {
		Strcat_char(os, (char)*p);
		p++;
	    } else
		(*st.ces_info->push_to)(os, wtf_parse(&p), &st);
	}
	break;
    }

    wc_push_end(os, &st);

    return os;
}

pStr
wc_Str_conv_with_detect(pStr is, wc_ces *f_ces, wc_ces hint, wc_ces t_ces)
{
    wc_ces detect;

    if (*f_ces == WC_CES_WTF || hint == WC_CES_WTF) {
	*f_ces = WC_CES_WTF;
	detect = WC_CES_WTF;
    } else if (WcOption.auto_detect == WC_OPT_DETECT_OFF) {
	*f_ces = hint;
	detect = hint;
    } else {
	if (*f_ces & WC_CES_T_8BIT)
	    hint = *f_ces;
	detect = wc_auto_detect(is->ptr, is->length, hint);
	if (WcOption.auto_detect == WC_OPT_DETECT_ON) {
	    if ((detect & WC_CES_T_8BIT) ||
		((detect & WC_CES_T_NASCII) && ! (*f_ces & WC_CES_T_8BIT)))
		*f_ces = detect;
	} else {
	    if ((detect & WC_CES_T_ISO_2022) && ! (*f_ces & WC_CES_T_8BIT))
		*f_ces = detect;
	}
    }
    return wc_Str_conv(is, detect, t_ces);
}

void
wc_push_end(pStr os, wc_status *st)
{
    if (st->ces_info->id & WC_CES_T_ISO_2022)
	wc_push_to_iso2022_end(os, st);
    else if (st->ces_info->id == WC_CES_HZ_GB_2312)
	wc_push_to_hz_end(os, st);
#ifdef USE_UNICODE
    else if (st->ces_info->id == WC_CES_UTF_8)
	wc_push_to_utf8_end(os, st);
    else if (st->ces_info->id == WC_CES_UTF_7)
	wc_push_to_utf7_end(os, st);
#endif
}

