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
#pragma once
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

struct Str {
    char* ptr;
    int len;
    int capacity;
};
typedef struct Str* pStr;

pStr Strnew(void);

struct Str Str_alloc(int n);

inline static pStr Strnew_size(int n)
{
    pStr x = Strnew();
    *x = Str_alloc(n);
    return x;
}

pStr Strcopy_charp_n(pStr, const char*, int);
pStr Strnew_charp_n(const char*, int);
pStr Strnew_charp(const char*);

pStr Strnew_m_charp(const char*, ...);
pStr Strdup(pStr);
pStr Strclear(pStr);
void Strfree(pStr);
pStr Strcopy(pStr dst, pStr src);
pStr Strcopy_charp(pStr, const char*);
pStr Strcat_charp_n(pStr, const char*, int);
pStr Strcat(pStr, pStr);
pStr Strcat_charp(pStr, const char*);
pStr Strcat_m_charp(pStr, ...);
pStr Strsubstr(pStr, int, int);
pStr Strinsert_char(pStr, int, char);
pStr Strinsert_charp(pStr, int, const char*);
pStr Strinsert_charp_n(pStr s, int pos, const char* p, int n);
pStr Strdelete(pStr, int, int);
pStr Strtruncate(pStr, int);
pStr Strlower(pStr);
pStr Strupper(pStr);
pStr Strchop(pStr);
pStr Strshrink(pStr, int);
pStr Strremovefirstspaces(pStr);
pStr Strremovetrailingspaces(pStr);
pStr Stralign_left(pStr, int);
pStr Stralign_right(pStr, int);
pStr Stralign_center(pStr, int);

#ifdef GNUC
#define FORMAT_PRINTF(fmt, arg) attribute((format(printf, fmt, arg)))
#else
#define FORMAT_PRINTF(fmt, arg)
#endif
pStr Sprintf(const char* fmt, ...) FORMAT_PRINTF(1, 2);

pStr Strfgets(FILE*);
pStr Strfgetall(FILE*);

pStr Strgrow(pStr s);

inline static pStr Strcat_char(pStr x, char y) { return Strinsert_char(x, (x)->len, y); }
inline static int Strcmp(pStr x, pStr y)
{
    return strcmp((x)->ptr, (y)->ptr);
}
inline static int Strcmp_charp(pStr x, const char* y)
{
    return strcmp((x)->ptr, (y));
}
inline static int Strcasecmp(pStr x, pStr y)
{
    return strcasecmp((x)->ptr, (y)->ptr);
}
inline static int Strcasecmp_charp(pStr x, const char* y)
{
    return strcasecmp((x)->ptr, (y));
}
inline static int Strncasecmp_charp(pStr x, const char* y, int n)
{
    return strncasecmp((x)->ptr, (y), (n));
}
inline static char Strlastchar(pStr s)
{
    return ((s)->len > 0 ? (s)->ptr[(s)->len - 1] : '\0');
}
inline static void Strshrinkfirst(pStr s, int n)
{
    Strdelete((s), 0, (n));
}
inline static void Strfputs(pStr s, FILE* f)
{
    fwrite((s)->ptr, 1, (s)->len, (f));
}

char* allocStr(const char* s, int len);
