
#include "gc_helper.h"
#include "ctrlcode.h"
#include "symbol.h"
#include "Symbols/alt.sym"
#include "Symbols/graph.sym"
#include "Symbols/eucjp.sym"
#include "Symbols/euckr.sym"
#include "Symbols/euccn.sym"
#include "Symbols/euctw.sym"
#include "Symbols/big5.sym"
#include "Symbols/utf8.sym"
#include "Symbols/cp850.sym"
#include "wtf.h"
#include "w3m.h"
#include <tcb/span.hpp>

#include "indep.h"

struct symbol_set
{
    CharacterEncodingScheme ces;
    char width;
    const char **item;
    const char **conved_item;
};
static symbol_set alt_symbol_set = {WC_CES_US_ASCII, 1, alt_symbol, alt_symbol};
static symbol_set alt2_symbol_set = {WC_CES_US_ASCII, 2, alt2_symbol, alt2_symbol};
static symbol_set eucjp_symbol_set = {WC_CES_EUC_JP, 2, eucjp_symbol, NULL};
static symbol_set euckr_symbol_set = {WC_CES_EUC_KR, 2, euckr_symbol, NULL};
static symbol_set euccn_symbol_set = {WC_CES_EUC_CN, 2, euccn_symbol, NULL};
static symbol_set euctw_symbol_set = {WC_CES_EUC_TW, 2, euctw_symbol, NULL};
static symbol_set big5_symbol_set = {WC_CES_BIG5, 2, big5_symbol, NULL};
static symbol_set utf8_symbol_set = {WC_CES_UTF_8, 1, utf8_symbol, NULL};
static symbol_set cp850_symbol_set = {WC_CES_CP850, 1, cp850_symbol, NULL};

struct charset_symbol_set
{
    CharacterEncodingScheme charset;
    symbol_set *symbol;
};
static charset_symbol_set charset_symbol_list[] = {
    {WC_CES_EUC_JP, &eucjp_symbol_set},
    {WC_CES_SHIFT_JIS, &eucjp_symbol_set},
    {WC_CES_ISO_2022_JP, &eucjp_symbol_set},
    {WC_CES_ISO_2022_JP_2, &eucjp_symbol_set},
    {WC_CES_ISO_2022_JP_3, &eucjp_symbol_set},
    {WC_CES_EUC_KR, &euckr_symbol_set},
    {WC_CES_ISO_2022_KR, &euckr_symbol_set},
    {WC_CES_JOHAB, &euckr_symbol_set},
    {WC_CES_UHC, &euckr_symbol_set},
    {WC_CES_EUC_CN, &euccn_symbol_set},
    {WC_CES_GBK, &euccn_symbol_set},
    {WC_CES_GB18030, &euccn_symbol_set},
    {WC_CES_HZ_GB_2312, &euccn_symbol_set},
    {WC_CES_ISO_2022_CN, &euccn_symbol_set},
    {WC_CES_EUC_TW, &euctw_symbol_set},
    {WC_CES_BIG5, &big5_symbol_set},
    {WC_CES_HKSCS, &big5_symbol_set},

    {WC_CES_UTF_8, &utf8_symbol_set},

    {WC_CES_CP850, &cp850_symbol_set},
    {WC_CES_NONE, NULL},
};
/* *INDENT-ON* */

static CharacterEncodingScheme save_charset = WC_CES_NONE;
static symbol_set *save_symbol = NULL;

static void
encode_symbol(symbol_set *s)
{
    int i;

    for (i = 0; s->item[i]; i++)
        ;
    s->conved_item = New_N(const char *, i);
    for (i = 0; s->item[i]; i++)
    {
        if (*(s->item[i]))
            s->conved_item[i] = wc_conv(s->item[i], s->ces, w3mApp::Instance().InnerCharset)->ptr;
    }
}

const char **
get_symbol(CharacterEncodingScheme charset, int *width)
{
    charset_symbol_set *p;
    symbol_set *s = NULL;

    if (w3mApp::Instance().UseGraphicChar != GRAPHIC_CHAR_ASCII)
    {
        if (charset == save_charset && save_symbol != NULL &&
            *width == save_symbol->width)
        {
            return save_symbol->conved_item;
        }
        save_charset = charset;
        for (p = charset_symbol_list; p->charset; p++)
        {
            if (charset == p->charset &&
                (*width == 0 || *width == p->symbol->width))
            {
                s = p->symbol;
                break;
            }
        }
        if (s == NULL)
            s = (*width == 2) ? &alt2_symbol_set : &alt_symbol_set;
        if (s != save_symbol)
        {
            if (!s->conved_item)
                encode_symbol(s);
            save_symbol = s;
        }
    }
    else
    {
        if (save_symbol != NULL && *width == save_symbol->width)
            return save_symbol->conved_item;
        s = (*width == 2) ? &alt2_symbol_set : &alt_symbol_set;
        save_symbol = s;
    }
    *width = s->width;
    return s->conved_item;
}

template <typename T>
int count_until_null(const T *p)
{
    int i = 0;
    for (; *p; ++p)
        ++i;
    return i;
}

static tcb::span<const char *> set_symbol(int width)
{
    static std::vector<const char *> symbol_buf;
    static int save_width = -1;
    if (width != save_width)
    {
        symbol_buf.clear();
        {
            symbol_set *s = &alt_symbol_set;
            for (int i = 0; s->item[i]; i++)
            {
                auto tmp = Strnew_size(4);
                if (width == 2)
                    wtf_push(tmp, WC_CCS_SPECIAL_W, (uint32_t)(SYMBOL_BASE + i));
                else
                    wtf_push(tmp, WC_CCS_SPECIAL, (uint32_t)(SYMBOL_BASE + i));
                symbol_buf.push_back(tmp->ptr);
            }
        }
    }
    save_width = width;
    return symbol_buf;
}

std::string_view get_width_symbol(int width, char symbol)
{
    auto map = set_symbol(width);
    return map[symbol];
}

void update_utf8_symbol(void)
{
    charset_symbol_set *p;
    utf8_symbol_set.width = WcOption.east_asian_width ? 2 : 1;
    for (p = charset_symbol_list; p->charset; p++)
    {
        if (p->charset == WC_CES_UTF_8)
        {
            encode_symbol(p->symbol);
            break;
        }
    }
}

void push_symbol(Str str, char symbol, int width, int n)
{
    const char *p;
    if (width == 2)
        p = alt2_symbol[(int)symbol];
    else
        p = alt_symbol[(int)symbol];

    const char buf[] =
        {
            (p[0] == ' ') ? (char)NBSP_CODE : p[0],
            (p[1] == ' ') ? (char)NBSP_CODE : p[1],
        };

    str->Push(Sprintf("<_SYMBOL TYPE=%d>", symbol));
    for (int i = 0; i < n; ++i)
    {
        str->Push(buf, 2);
    }
    str->Push("</_SYMBOL>");
}
