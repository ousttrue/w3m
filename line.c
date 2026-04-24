#include "line.h"
#include "display.h"
#include "wc_util.h"

void cleanup_line(Str s, enum LineMode mode)
{
    if (s->length >= 2 && s->ptr[s->length - 2] == '\r' && s->ptr[s->length - 1] == '\n') {
        Strshrink(s, 2);
        Strcat_char(s, '\n');
    } else if (Strlastchar(s) == '\r')
        s->ptr[s->length - 1] = '\n';
    else if (Strlastchar(s) != '\n')
        Strcat_char(s, '\n');
    if (mode != PAGER_MODE) {
        int i;
        for (i = 0; i < s->length; i++) {
            if (s->ptr[i] == '\0')
                s->ptr[i] = ' ';
        }
    }
}

/// do_chop if SCM_NEWS
Str convertLine(const uint8_t* p, int len, enum LineMode mode, wc_ces* charset, wc_ces doc_charset, bool do_chop)
{
    Str line = Strnew_wc_output(wc_Str_conv_with_detect(WcOption, p, len, charset, doc_charset, InnerCharset));
    if (mode != RAW_MODE)
        cleanup_line(line, mode);
    if (do_chop)
        Strchop(line);
    return line;
}

void addStr(char* p, Lineprop* pr, int len, int offset, int limit)
{
    int i = 0, rcol = 0, ncol, delta = 1;

    if (offset) {
        for (i = 0; i < len; i++) {
            if (calcPosition(p, pr, len, i, 0, CP_AUTO) > offset)
                break;
        }
        if (i >= len)
            return;
        while (pr[i] & PC_WCHAR2)
            i++;
        addChar('{', 0);
        rcol = offset + 1;
        ncol = calcPosition(p, pr, len, i, 0, CP_AUTO);
        for (; rcol < ncol; rcol++)
            addChar(' ', 0);
    }
    for (; i < len; i += delta) {
        delta = wtf_len((wc_uchar*)&p[i]);
        ncol = calcPosition(p, pr, len, i + delta, 0, CP_AUTO);
        if (ncol - offset > limit)
            break;
        if (p[i] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(' ', 0);
            continue;
        } else {
            addMChar(&p[i], pr[i], delta);
        }
        rcol = ncol;
    }
}

void addPasswd(char* p, Lineprop* pr, int len, int offset, int limit)
{
    int rcol = 0;
    int ncol = calcPosition(p, pr, len, len, 0, CP_AUTO);
    if (ncol > offset + limit)
        ncol = offset + limit;
    if (offset) {
        addChar('{', 0);
        rcol = offset + 1;
    }
    for (; rcol < ncol; rcol++)
        addChar('*', 0);
}
