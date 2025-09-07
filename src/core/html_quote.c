#include "html_quote.h"
#include "quote.h"
#include "entity.h"

const char* HTML_QUOTE_MAP[] = {
    0,
    "&amp;",
    "&lt;",
    "&gt;",
    "&quot;",
    "&apos;",
    0,
    0,
};

const char* html_quote_char(unsigned char c) { return HTML_QUOTE_MAP[(int)is_html_quote(c)]; }

const char* html_quote(const char* str)
{
    Str tmp = 0;
    const char* p;
    for (p = str; *p; p++) {
        const char* q = html_quote_char(*p);
        if (q) {
            if (tmp == 0)
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

const char* html_unquote(const char* str)
{
    Str tmp = 0;
    const char *p, *q;
    for (p = str; *p;) {
        if (*p == '&') {
            if (tmp == 0)
                tmp = Strnew_charp_n(str, (int)(p - str));
            q = getescapecmd(&p);
            Strcat_charp(tmp, q);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
            p++;
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}
