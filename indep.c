#include "indep.h"
#include "Str.h"
#include "myctype.h"

char* html_quote(const char* str)
{
    Str tmp = NULL;
    const char *p, *q;

    for (p = str; *p; p++) {
        q = html_quote_char(*p);
        if (q) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_charp(tmp, q);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}
