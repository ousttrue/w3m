#include "line.h"
#include "alloc.h"
#include "w3m_rc.h"

static int
nextColumn(int n, char* p, Lineprop* pr, int Tabstop)
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
        j = nextColumn(j, &l[i], &pr[i], getRuntime()->Tabstop);
        i++;
        for (; i < len && pr[i] & PC_WCHAR2; i++)
            realColumn[i] = realColumn[i - 1];
    }
    if (pos >= i)
        return j;
    return realColumn[pos];
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

int columnLen(struct Line* line, int column)
{
    int i, j;
    for (i = 0, j = 0; i < line->len;) {
        int j = nextColumn(j, &line->lineBuf[i], &line->propBuf[i], getRuntime()->Tabstop);
        if (j > column)
            return i;
        i++;
        while (i < line->len && line->propBuf[i] & PC_WCHAR2)
            i++;
    }
    return line->len;
}
