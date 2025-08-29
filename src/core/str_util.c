#include "str_util.h"
#include "ctrlcode.h"

Str escape_spaces(Str s)
{
    if (s == NULL)
        return s;

    Str tmp = NULL;
    char* p;
    for (p = s->ptr; *p; p++) {
        if (*p == ' ' || *p == CTRL_I) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
            Strcat_char(tmp, '\\');
        }
        if (tmp)
            Strcat_char(tmp, *p);
    }
    if (tmp)
        return tmp;
    return s;
}

Str unescape_spaces(Str s)
{
    if (s == NULL)
        return s;

    Str tmp = NULL;
    char* p;
    for (p = s->ptr; *p; p++) {
        if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return s;
}
