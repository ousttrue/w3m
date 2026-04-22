#include "LineInput.h"

struct LineInput LineInputInit(const char* def_str)
{
    struct LineInput li = {
        .is_passwd = false,
        .move_word = true,
        .strBuf = Strnew(),
        .CLen = 0,
        .CPos = 0,
    };
    if (def_str) {
        li.strBuf = Strnew_charp(def_str);
        setStrType(&li);
    }
    return li;
}

void setStrType(struct LineInput* li)
{
    Lineprop ctype;
    char* p = li->strBuf->ptr;
    char* ep = p + li->strBuf->length;
    int i = 0;
    for (; p < ep;) {
        int len = get_mclen(p);
        if (i + len > STR_LEN)
            break;
        ctype = get_mctype(p);
        if (li->is_passwd) {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        li->strProp[i++] = ctype;
        p += len;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--)
                li->strProp[i++] = ctype;
        }
    }
    li->CLen = i;
    li->CPos = i;
}
