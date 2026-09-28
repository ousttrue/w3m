#include "str_gc.h"

Str mydirname(const char* s)
{
    const char* p = s;
    while (*p)
        p++;
    if (s != p)
        p--;
    while (s != p && *p == '/')
        p--;
    while (s != p && *p != '/')
        p--;
    if (*p != '/')
        return Strnew_charp(".");
    while (s != p && *p == '/')
        p--;
    return Strnew_charp_n(s, strlen(s) - strlen(p) + 1);
}
