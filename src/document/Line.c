#include "Line.h"
#include "alloc.h"
#include "ctrlcode.h"
#include <stdlib.h>
#include <string.h>

#include <wc.h>
#include <wtf.h>

Lineprop get_mctype(const char *c){
    return ((Lineprop)wtf_type((wc_uchar*)(c)) << 8);
}

int Tabstop = 8;

struct LineList* newLine(char* line, Lineprop* prop, Linecolor* color, int pos, int index)
{
    struct LineList* l = New(struct LineList);
    l->next = NULL;
    l->prev = NULL;
    l->bpos = 0;
    l->bwidth = 0;
    l->linenumber = index;
    l->l = (struct Line) {
        .lineBuf = line,
        .propBuf = prop,
        .colorBuf = color,
        .len = pos,
        .width = -1,
        .size = pos,
    };
    return l;
}

int columnPos(struct Line* line, int column)
{
    int i;

    for (i = 1; i < line->len; i++) {
        if (COLPOS(line, i) > column)
            break;
    }
    for (i--; i > 0 && line->propBuf[i] & PC_WCHAR2; i--)
        ;
    return i;
}

static int
nextColumn(int n, char* p, Lineprop* pr)
{
    if (*pr & PC_CTRL) {
        if (*p == '\t')
            return (n + Tabstop) / Tabstop * Tabstop;
        else if (*p == '\n')
            return n + 1;
        else if (*p != '\r')
            return n + 2;
        return n;
    }
    if (*pr & PC_UNKNOWN)
        return n + 4;
    return n + wtf_width((wc_uchar*)p);
}

int calcPosition(char* l, Lineprop* pr, int len, int pos, int bpos, enum CalcPositionMode mode)
{
    static int* realColumn = 0;
    static int size = 0;
    static char* prevl = 0;
    int i, j;

    if (l == 0 || len == 0 || pos < 0)
        return bpos;
    if (l == prevl && mode == CP_AUTO) {
        if (pos <= len)
            return realColumn[pos];
    }
    if (size < len + 1) {
        size = (len + 1 > LINELEN) ? (len + 1) : LINELEN;
        realColumn = New_N(int, size);
    }
    prevl = l;
    i = 0;
    j = bpos;
    if (pr[i] & PC_WCHAR2) {
        for (; i < len && pr[i] & PC_WCHAR2; i++)
            realColumn[i] = j;
        if (i > 0 && pr[i - 1] & PC_KANJI && WcOption.use_wide)
            j++;
    }
    while (1) {
        realColumn[i] = j;
        if (i == len)
            break;
        j = nextColumn(j, &l[i], &pr[i]);
        i++;
        for (; i < len && pr[i] & PC_WCHAR2; i++)
            realColumn[i] = realColumn[i - 1];
    }
    if (pos >= i)
        return j;
    return realColumn[pos];
}

int columnLen(struct Line* line, int column)
{
    int i, j;

    for (i = 0, j = 0; i < line->len;) {
        j = nextColumn(j, &line->lineBuf[i], &line->propBuf[i]);
        if (j > column)
            return i;
        i++;
        while (i < line->len && line->propBuf[i] & PC_WCHAR2)
            i++;
    }
    return line->len;
}

// extact escape sequence
static bool
parse_ansi_color(const char** str, Lineprop* effect, Linecolor* color)
{
    const char* p = *str;
    if (*p != ESC_CODE || *(p + 1) != '[')
        return 0;
    p += 2;

    Lineprop e = *effect;
    Linecolor c = *color;

    const char* q;
    for (q = p; IS_DIGIT(*q) || *q == ';'; q++)
        ;
    if (*q != 'm')
        return 0;

    *str = q + 1;
    while (1) {
        if (*p == 'm') {
            e = PE_NORMAL;
            c = 0;
            break;
        }
        if (IS_DIGIT(*p)) {
            q = p;
            for (p++; IS_DIGIT(*p); p++)
                ;
            int i = atoi(allocStr(q, p - q));
            switch (i) {
            case 0:
                e = PE_NORMAL;
                c = 0;
                break;
            case 1:
            case 5:
                e = PE_BOLD;
                break;
            case 4:
                e = PE_UNDER;
                break;
            case 7:
                e = PE_STAND;
                break;
            case 100: /* for EWS4800 kterm */
                c = 0;
                break;
            case 39:
                c &= 0xf0;
                break;
            case 49:
                c &= 0x0f;
                break;
            default:
                if (i >= 30 && i <= 37)
                    c = (c & 0xf0) | (i - 30) | 0x08;
                else if (i >= 40 && i <= 47)
                    c = (c & 0x0f) | ((i - 40) << 4) | 0x80;
                break;
            }
            if (*p == 'm')
                break;
        } else {
            e = PE_NORMAL;
            c = 0;
            break;
        }
        p++; /* *p == ';' */
    }
    *effect = e;
    *color = c;
    return true;
}

Str checkType(Str s, Lineprop** oprop, Linecolor** ocolor)
{
    static Lineprop* prop_buffer = NULL;
    static int prop_size = 0;
    static Linecolor* color_buffer = NULL;
    static int color_size = 0;
    static int* plens_buffer = NULL;
    static int plens_size = 0;

    Lineprop mode;
    Lineprop effect = PE_NORMAL;
    Lineprop* prop;
    char *str = s->ptr, *endp = &s->ptr[s->length], *bs = NULL;
    Lineprop ceffect = PE_NORMAL;
    Linecolor cmode = 0;
    int check_color = false;
    Linecolor* color = NULL;
    char* es = NULL;
    int do_copy = false;
    int i;
    int plen = 0, clen;
    int* plens = NULL;

    if (prop_size < s->length) {
        prop_size = (s->length > LINELEN) ? s->length : LINELEN;
        prop_buffer = New_Reuse(Lineprop, prop_buffer, prop_size);
    }
    prop = prop_buffer;
    if (plens_size < s->length) {
        plens_size = (s->length > LINELEN) ? s->length : LINELEN;
        plens_buffer = New_Reuse(int, plens_buffer, plens_size);
    }
    plens = plens_buffer;

    bs = memchr(str, '\b', s->length);
    if (ocolor) {
        es = memchr(str, ESC_CODE, s->length);
        if (es) {
            if (color_size < s->length) {
                color_size = (s->length > LINELEN) ? s->length : LINELEN;
                color_buffer = New_Reuse(Linecolor, color_buffer,
                    color_size);
            }
            color = color_buffer;
        }
    }
    if ((bs != NULL)
        || (es != NULL)) {
        char *sp = str, *ep;
        s = Strnew_size(s->length);
        do_copy = true;
        ep = endp;
        if (bs && ep > bs - 2)
            ep = bs - 2;
        if (es && ep > es - 2)
            ep = es - 2;
        for (; str < ep && IS_ASCII(*str); str++) {
            *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
            if (color)
                *(color++) = 0;
            *(plens++) = plen = 1;
        }
        Strcat_charp_n(s, sp, (int)(str - sp));
    }

    if (!do_copy) {
        for (; str < endp && IS_ASCII(*str); str++) {
            *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
            if (color)
                *(color++) = 0;
            *(plens++) = plen = 1;
        }
    }

    while (str < endp) {
        if (prop - prop_buffer >= prop_size)
            break;
        if (bs != NULL) {
            if (str == bs - 2 && !strncmp(str, "__\b\b", 4)) {
                str += 4;
                effect = PE_UNDER;
                if (str < endp)
                    bs = memchr(str, '\b', endp - str);
                continue;
            } else if (str == bs - 1 && *str == '_') {
                str += 2;
                effect = PE_UNDER;
                if (str < endp)
                    bs = memchr(str, '\b', endp - str);
                continue;
            } else if (str == bs) {
                if (*(str + 1) == '_') {
                    if (s->length) {
                        str += 2;
                        for (i = 1; i <= plen; i++)
                            *(prop - i) |= PE_UNDER;
                    } else {
                        str++;
                    }
                } else if (!strncmp(str + 1, "\b__", 3)) {
                    if (s->length) {
                        str += (plen == 1) ? 3 : 4;
                        for (i = 1; i <= plen; i++)
                            *(prop - i) |= PE_UNDER;
                    } else {
                        str += 2;
                    }
                } else if (*(str + 1) == '\b') {
                    if (s->length) {
                        clen = get_mclen(str + 2);
                        if (plen == clen && !strncmp(str - plen, str + 2, plen)) {
                            for (i = 1; i <= plen; i++)
                                *(prop - i) |= PE_BOLD;
                            str += 2 + clen;
                        } else {
                            Strshrink(s, plen);
                            prop -= plen;
                            if (color)
                                color -= plen;
                            if (plens == plens_buffer)
                                plen = 0;
                            else
                                plen = *(--plens);
                            str += 2;
                        }
                    } else {
                        str += 2;
                    }
                } else {
                    if (s->length) {
                        clen = get_mclen(str + 1);
                        if (plen == clen && !strncmp(str - plen, str + 1, plen)) {
                            for (i = 1; i <= plen; i++)
                                *(prop - i) |= PE_BOLD;
                            str += 1 + clen;
                        } else {
                            Strshrink(s, plen);
                            prop -= plen;
                            if (color)
                                color -= plen;
                            if (plens == plens_buffer)
                                plen = 0;
                            else
                                plen = *(--plens);
                            str++;
                        }
                    } else {
                        str++;
                    }
                }
                if (str < endp)
                    bs = memchr(str, '\b', endp - str);
                continue;
            } else if (str > bs)
                bs = memchr(str, '\b', endp - str);
        }
        if (es != NULL) {
            if (str == es) {
                int ok = parse_ansi_color(&str, &ceffect, &cmode);
                if (str < endp)
                    es = memchr(str, ESC_CODE, endp - str);
                if (ok) {
                    if (cmode)
                        check_color = true;
                    continue;
                }
            } else if (str > es)
                es = memchr(str, ESC_CODE, endp - str);
        }

        mode = get_mctype(str) | effect;
        if (color) {
            *(color++) = cmode;
            mode |= ceffect;
        }
        *(prop++) = mode;
        plen = get_mclen(str);
        if (str + plen > endp)
            plen = endp - str;
        *(plens++) = plen;
        if (plen > 1) {
            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
            for (i = 1; i < plen; i++) {
                *(prop++) = mode;
                if (color)
                    *(color++) = cmode;
            }
            if (do_copy)
                Strcat_charp_n(s, (char*)str, plen);
            str += plen;
        } else {
            if (do_copy)
                Strcat_char(s, (char)*str);
            str++;
        }
        effect = PE_NORMAL;
    }
    *oprop = prop_buffer;
    if (ocolor)
        *ocolor = check_color ? color_buffer : NULL;
    return s;
}

int nextChar(int s, struct Line* l)
{
    do {
        (s)++;
    } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2);
    return s;
}

int prevChar(int s, struct Line* l)
{
    do {
        (s)--;
    } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2);
    return s;
}

