/*
 * String manipulation library for Boehm GC
 *
 * (C) Copyright 1998-1999 by Akinori Ito
 *
 * This software may be redistributed freely for this purpose, in full
 * or in part, provided that this entire copyright notice is included
 * on any copies of this software and applications and derivations thereof.
 *
 * This software is provided on an "as is" basis, without warranty of any
 * kind, either expressed or implied, as to any matter including, but not
 * limited to warranty of fitness of purpose, or merchantability, or
 * results obtained from use of this software.
 */
#include "Str.h"
#include "myctype.h"

#include <gc/gc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMX__ /* or include "fm.h" for HAVE_BCOPY? */
#include <strings.h>
#endif

#define INITIAL_STR_SIZE 32
#define STR_SIZE_MAX (STR_LEN_MAX + 1)
#define STR_LEN_MAX (UINT_MAX / 32 - 1)

pStr Strnew(void)
{
    pStr x = GC_MALLOC(sizeof(struct Str));
    if (!x)
        exit(3);
    *x = Str_alloc(INITIAL_STR_SIZE - 1);
    return x;
}

struct Str Str_alloc(int n)
{
    if (n < 0 || n > STR_LEN_MAX)
        n = STR_SIZE_MAX;
    else if (n + 1 < INITIAL_STR_SIZE)
        n = INITIAL_STR_SIZE;
    else
        n++;
    struct Str str = {
        .ptr = GC_MALLOC_ATOMIC(n),
        .capacity = n,
        .len = 0,
    };
    if (!str.ptr)
        exit(3);
    str.ptr[0] = '\0';
    return str;
}

static void Strgrow_n(pStr x, int n)
{
    if (n < 0)
        n = STR_SIZE_MAX;
    else
        n = (n >= STR_SIZE_MAX) ? STR_SIZE_MAX : n + 1;

    if (x->capacity >= n)
        return;

    if (!(x->ptr = GC_REALLOC(x->capacity ? x->ptr : NULL, n)))
        exit(3);
    x->capacity = n;
}

pStr Strclear(pStr s)
{
    s->len = 0;
    s->ptr[s->len] = '\0';
    return s;
}

void Strfree(pStr x)
{
    if (!x)
        return;
    GC_free(x->ptr);
    GC_free(x);
}

//
pStr Strcopy_charp_n(pStr x, const char* y, int n)
{
    if (!x)
        x = Strnew();
    if (!y)
        return Strtruncate(x, 0);

    if (n > STR_LEN_MAX)
        n = STR_LEN_MAX;
    if (x->capacity <= n)
        Strgrow_n(x, n);
    memmove(x->ptr, y, n);
    x->len = n;
    x->ptr[x->len] = '\0';
    return x;
}

pStr Strnew_charp_n(const char* p, int n)
{
    pStr x = Strnew_size(n);
    if (p)
        Strcopy_charp_n(x, p, n);
    return x;
}

pStr Strnew_charp(const char* p)
{
    return p ? Strnew_charp_n(p, strlen(p)) : Strnew();
}

pStr Strnew_m_charp(const char* p, ...)
{
    va_list ap;
    pStr r = Strnew();
    va_start(ap, p);
    while (p != NULL) {
        Strcat_charp(r, p);
        p = va_arg(ap, char*);
    }
    va_end(ap);
    return r;
}

pStr Strdup(pStr s)
{
    pStr n = Strnew_size(s->len);
    Strcopy(n, s);
    return n;
}

pStr Strcopy(pStr dst, pStr src)
{
    return Strcopy_charp_n(dst, src->ptr, src->len);
}

pStr Strcopy_charp(pStr x, const char* y)
{
    if (!y)
        return Strtruncate(x, 0);
    int len = strlen(y);
    return Strcopy_charp_n(x, y, len);
}

pStr Strcat_charp_n(pStr x, const char* y, int n)
{
    if (!y || !n || (x && x->len == STR_LEN_MAX))
        return x;
    if (!x)
        x = Strnew();
    if (n < 0)
        n = strlen(y);
    int newlen = x->len + n;
    if (newlen > STR_LEN_MAX) {
        newlen = STR_LEN_MAX;
        n = STR_LEN_MAX - x->len;
    }

    if (newlen >= x->capacity)
        Strgrow_n(x, newlen);
    memmove(&x->ptr[x->len], y, n);
    x->len += n;
    x->ptr[x->len] = '\0';
    return x;
}

pStr Strcat(pStr x, pStr y)
{
    return Strcat_charp_n(x, y->ptr, y->len);
}

pStr Strcat_charp(pStr x, const char* y)
{
    if (!y)
        return x;
    return Strcat_charp_n(x, y, strlen(y));
}

pStr Strcat_m_charp(pStr x, ...)
{
    va_list ap;
    char* p;
    if (!x)
        x = Strnew();
    va_start(ap, x);
    while ((p = va_arg(ap, char*)) != NULL)
        Strcat_charp_n(x, p, strlen(p));
    va_end(ap);
    return x;
}

pStr Strgrow(pStr x)
{
    int newlen, addlen;

    if (x->capacity < 8192)
        addlen = x->capacity;
    else
        addlen = x->capacity / 2;
    if (addlen < INITIAL_STR_SIZE)
        addlen = INITIAL_STR_SIZE;
    newlen = x->capacity + addlen;
    if (newlen <= 0 || newlen > STR_SIZE_MAX) {
        newlen = STR_SIZE_MAX;
        if (x->len + 1 >= newlen)
            x->len = newlen - 2;
    }
    Strgrow_n(x, newlen - 1);
    return x;
}

pStr Strsubstr(pStr s, int beg, int len)
{
    pStr new_s;
    int i;

    new_s = Strnew();
    if (beg >= s->len)
        return new_s;
    for (i = 0; i < len && beg + i < s->len; i++)
        Strcat_char(new_s, s->ptr[beg + i]);
    return new_s;
}

pStr Strlower(pStr s)
{
    for (int i = 0; i < s->len; i++)
        s->ptr[i] = TOLOWER(s->ptr[i]);
    return s;
}

pStr Strupper(pStr s)
{
    for (int i = 0; i < s->len; i++)
        s->ptr[i] = TOUPPER(s->ptr[i]);
    return s;
}

pStr Strchop(pStr s)
{
    while (s->len > 0 && (s->ptr[s->len - 1] == '\n' || s->ptr[s->len - 1] == '\r')) {
        s->len--;
    }
    s->ptr[s->len] = '\0';
    return s;
}

pStr Strinsert_char(pStr s, int pos, char c)
{
    if (pos < 0 || s->len < pos || s->len == STR_LEN_MAX)
        return s;
    if (s->len + 1 >= s->capacity)
        Strgrow(s);
    for (int i = s->len; i > pos; i--)
        s->ptr[i] = s->ptr[i - 1];
    s->len++;
    s->ptr[s->len] = '\0';
    s->ptr[pos] = c;
    return s;
}

pStr Strinsert_charp_n(pStr s, int pos, const char* p, int n)
{
    while (n--)
        Strinsert_char(s, pos++, *(p++));
    return s;
}

pStr Strinsert_charp(pStr s, int pos, const char* p)
{
    while (*p)
        Strinsert_char(s, pos++, *(p++));
    return s;
}

pStr Strdelete(pStr s, int pos, int n)
{
    if (pos < 0 || s->len < pos)
        return s;
    if (n < 0)
        n = STR_LEN_MAX - pos;
    if (s->len <= pos + n) {
        s->len = pos;
        s->ptr[s->len] = '\0';
        return s;
    }
    int i;
    for (i = pos; i < s->len - n; i++)
        s->ptr[i] = s->ptr[i + n];
    s->len = i;
    s->ptr[s->len] = '\0';
    return s;
}

pStr Strtruncate(pStr s, int pos)
{
    if (pos < 0 || s->len < pos)
        return s;
    s->len = pos;
    s->ptr[s->len] = '\0';
    return s;
}

pStr Strshrink(pStr s, int n)
{
    s->len = (n >= s->len) ? 0 : (s->len - n);
    s->ptr[s->len] = '\0';
    return s;
}

pStr Strremovefirstspaces(pStr s)
{
    int i;
    for (i = 0; i < s->len && IS_SPACE(s->ptr[i]); i++)
        ;
    if (i == 0)
        return s;
    return Strdelete(s, 0, i);
}

pStr Strremovetrailingspaces(pStr s)
{
    int i;
    for (i = s->len - 1; i >= 0 && IS_SPACE(s->ptr[i]); i--)
        ;
    s->len = i + 1;
    s->ptr[s->len] = '\0';
    return s;
}

pStr Stralign_left(pStr s, int width)
{
    if (s->len >= width)
        return Strdup(s);
    pStr n = Strnew_size(width);
    Strcopy(n, s);
    for (int i = s->len; i < width; i++)
        Strcat_char(n, ' ');
    return n;
}

pStr Stralign_right(pStr s, int width)
{
    if (s->len >= width)
        return Strdup(s);
    pStr n = Strnew_size(width);
    for (int i = s->len; i < width; i++)
        Strcat_char(n, ' ');
    Strcat(n, s);
    return n;
}

pStr Stralign_center(pStr s, int width)
{
    if (s->len >= width)
        return Strdup(s);
    pStr n = Strnew_size(width);
    int w = (width - s->len) / 2;
    for (int i = 0; i < w; i++)
        Strcat_char(n, ' ');
    Strcat(n, s);
    for (int i = w + s->len; i < width; i++)
        Strcat_char(n, ' ');
    return n;
}

pStr Sprintf(const char* fmt, ...)
{
    va_list ap, args;
    va_start(ap, fmt);
    va_copy(args, ap);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0)
        return Strnew();

    pStr s = Strnew_size(len);
    vsnprintf(s->ptr, s->capacity + 1, fmt, ap);
    va_end(ap);

    s->len = len;
    return s;
}

pStr Strfgets(FILE* f)
{
    pStr s = Strnew();
    int c;
    while ((c = fgetc(f)) != EOF) {
        Strcat_char(s, c);
        if (c == '\n')
            break;
    }
    return s;
}

pStr Strfgetall(FILE* f)
{
    pStr s = Strnew();
    int c;
    while ((c = fgetc(f)) != EOF) {
        Strcat_char(s, c);
    }
    return s;
}

struct Str allocStr_n(const char* s, int len)
{
    if (s == NULL)
        return (struct Str) { };
    if (len < 0)
        len = strlen(s);
    struct Str str = Str_alloc(len);
    memcpy(str.ptr, s, len);
    str.len = len;
    str.ptr[len] = '\0';
    return str;
}
