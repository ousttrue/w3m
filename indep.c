/* vi: set sw=4 ts=8 ai sm noet : */
#include "indep.h"

#include "Str.h"
#include "alloc.h"
#include "config.h"
#include "entity.h"
#include "fm.h"
#include "proto.h"
#include "myctype.h"
#include "pathdefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>

Str remove_space(const char* str)
{
    const char *p, *q;
    for (p = str; *p && IS_SPACE(*p); p++)
        ;
    for (q = p; *q; q++)
        ;
    for (; q > p && IS_SPACE(*(q - 1)); q--)
        ;
    if (*q != '\0')
        return Strnew_charp_n(p, q - p);
    return Strnew_charp(p);
}

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

Str Str_form_quote(Str x)
{
    Str tmp = NULL;
    char *p = x->ptr, *ep = x->ptr + x->length;
    char buf[4];

    for (; p < ep; p++) {
        if (*p == ' ') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            Strcat_char(tmp, '+');
        } else if (is_url_unsafe(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            sprintf(buf, "%%%02X", (unsigned char)*p);
            Strcat_charp(tmp, buf);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return x;
}

const char*
shell_quote(const char* str)
{
    Str tmp = NULL;
    const char* p;

    for (p = str; *p; p++) {
        if (is_shell_unsafe(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_char(tmp, '\\');
            Strcat_char(tmp, *p);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}
