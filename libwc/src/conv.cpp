#include "option.h"
#include "status.h"
#include "detect.h"
#include "conv.h"
#include "wtf.h"
#include "iso2022.h"
#include "hz.h"
#include "ucs.h"
#include "utf8.h"
#include "utf7.h"

const char *WC_REPLACE = "?";
const char *WC_REPLACE_W = "??";

SingleCharacter GetCharacter(CharacterEncodingScheme ces, const uint8_t **src)
{
    if (!src)
    {
        return SingleCharacter();
    }
    auto p = *src;
    if (!p)
    {
        return SingleCharacter();
    }
    if (p[0] == '\0')
    {
        return SingleCharacter();
    }

    switch (ces)
    {
    case WC_CES_US_ASCII:
    {
        ++(*src); // advance
        return SingleCharacter(p[0]);
    }

    case WC_CES_UTF_8:
    {
        // TODO: normalize
        {
            auto size = WC_UTF8_MAP[p[0]];
            (*src) += size; // advance
            return SingleCharacter(p, size);
        }
    }
    }

    return {};
}
SingleCharacter ToWtf(CharacterEncodingScheme ces, SingleCharacter src)
{
    return {""};
}
SingleCharacter FromWtf(CharacterEncodingScheme ces, SingleCharacter src)
{
    return {""};
}

static Str FromWtf(Str is, CharacterEncodingScheme ces)
{
    uint8_t *p;
    uint8_t *sp = (uint8_t *)is->ptr;
    uint8_t *ep = sp + is->Size();
    switch (ces)
    {
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
    {
        // all bytes is ascii range
        return is;
    }

    auto os = Strnew_size(is->Size());
    if (p > sp)
        p--; /* for precompose */
    if (p > sp)
        os->Push(is->ptr, (int)(p - sp));

    wc_status st;
    wc_output_init(ces, &st);
    switch (ces)
    {
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
        while (p < ep)
        {
            if (*p < 0x80 && wtf_width(p + 1))
            {
                os->Push((char)*p);
                p++;
            }
            else
                (*st.ces_info->push_to)(os, wtf_parse(&p), &st);
        }
        break;
    }
    wc_push_end(os, &st);

    return os;
}

///
///
///
Str wc_Str_conv(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    if (f_ces == WC_CES_WTF && t_ces == WC_CES_WTF)
    {
        // no conversion
        return is;
    }

    if (f_ces == WC_CES_WTF)
    {
        // is => wtf
        return FromWtf(is, t_ces);
    }

    // wtf <= is
    auto &info = GetCesInfo(f_ces);
    auto wtf = info.conv_from(is, f_ces);
    if (t_ces == WC_CES_WTF)
    {
        // wtf
        return wtf;
    }

    // wtf => to
    return FromWtf(wtf, t_ces);
}

Str wc_Str_conv_strict(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    Str os;
    wc_option opt = WcOption;

    WcOption.strict_iso2022 = true;
    WcOption.no_replace = true;
    WcOption.fix_width_conv = false;
    os = wc_Str_conv(is, f_ces, t_ces);
    WcOption = opt;
    return os;
}

Str wc_Str_conv_with_detect(Str is, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces)
{
    CharacterEncodingScheme detect;

    if (*f_ces == WC_CES_WTF || hint == WC_CES_WTF)
    {
        *f_ces = WC_CES_WTF;
        detect = WC_CES_WTF;
    }
    else if (WcOption.auto_detect == WC_OPT_DETECT_OFF)
    {
        *f_ces = hint;
        detect = hint;
    }
    else
    {
        if (*f_ces & WC_CES_T_8BIT)
            hint = *f_ces;
        detect = wc_auto_detect(is->ptr, is->Size(), hint);
        if (WcOption.auto_detect == WC_OPT_DETECT_ON)
        {
            if ((detect & WC_CES_T_8BIT) ||
                ((detect & WC_CES_T_NASCII) && !(*f_ces & WC_CES_T_8BIT)))
                *f_ces = detect;
        }
        else
        {
            if ((detect & WC_CES_T_ISO_2022) && !(*f_ces & WC_CES_T_8BIT))
                *f_ces = detect;
        }
    }
    return wc_Str_conv(is, detect, t_ces);
}

void wc_push_end(Str os, wc_status *st)
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

//
// Entity decode & character encoding ?
//
// https://dev.w3.org/html5/html-author/charref
//
// ucs4 codepoint 38 => '&'
//
const char *from_unicode(uint32_t codepoint, CharacterEncodingScheme ces)
{
    if (codepoint <= WC_C_UCS4_END)
    {
        uint8_t utf8[7];
        wc_ucs_to_utf8(codepoint, utf8);
        return wc_conv((char *)utf8, WC_CES_UTF_8, ces)->ptr;
    }

    return "?";
}

static CharacterEncodingScheme char_conv_t_ces = WC_CES_WTF;
static wc_status char_conv_st;

void wc_char_conv_init(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    wc_input_init(f_ces, &char_conv_st);
    char_conv_st.state = -1;
    char_conv_t_ces = t_ces;
}

Str wc_char_conv(char c)
{
    return wc_Str_conv((*char_conv_st.ces_info->char_conv)((uint8_t)c, &char_conv_st),
                       WC_CES_WTF, char_conv_t_ces);
}

static void write(Str str, FILE *f)
{
    if (str->Size())
    {
        fwrite(str->ptr, 1, str->Size(), f);
    }
}

WCWriter::WCWriter(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces, FILE *f)
    : m_f(f), m_from(f_ces), m_to(t_ces), m_status(new wc_status)
{
    wc_output_init(m_to, m_status);
    m_buffer = Strnew();
}

WCWriter::~WCWriter()
{
    delete m_status;
}

void WCWriter::end()
{
    m_buffer->Clear();
    wc_push_end(m_buffer, m_status);
    write(m_buffer, m_f);
}

void WCWriter::putc(const char *c, int len)
{
    if (c[len] != '\0')
    {
        c = Strnew_charp_n(c, len)->ptr;
    }
    auto *p = (uint8_t *)c;
    if (m_from != WC_CES_WTF)
    {
        // convert
        p = (uint8_t *)wc_conv(c, m_from, WC_CES_WTF)->ptr;
    }

    m_buffer->Clear();
    auto func = m_status->ces_info->push_to;
    while (*p)
    {
        auto c = wtf_parse(&p);
        func(m_buffer, c, m_status);
    }
    write(m_buffer, m_f);
}

void WCWriter::clear_status()
{
    // clear ISO-2022-JP escape sequence
    if (m_status->ces_info->id & WC_CES_T_ISO_2022)
    {
        m_status->gl = 0;
        m_status->gr = 0;
        m_status->ss = 0;
        m_status->design[0] = WC_CCS_NONE;
        m_status->design[1] = WC_CCS_NONE;
        m_status->design[2] = WC_CCS_NONE;
        m_status->design[3] = WC_CCS_NONE;
    }
}
