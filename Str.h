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

typedef struct Str {
    char* ptr;
    int length;
    int area_size;
}* Str;

Str Strnew(void);
Str Strnew_size(int);
Str Strnew_charp(const char*);
Str Strnew_charp_n(const char*, int);
Str Strnew_m_charp(const char*, ...);
Str Strdup(Str);
Str Strclear(Str);
void Strfree(Str);
Str Strcopy(Str dst, Str src);
Str Strcopy_charp(Str, const char*);
Str Strcopy_charp_n(Str, const char*, int);
Str Strcat_charp_n(Str, const char*, int);
Str Strcat(Str, Str);
Str Strcat_charp(Str, const char*);
Str Strcat_m_charp(Str, ...);
Str Strsubstr(Str, int, int);
Str Strinsert_char(Str, int, char);
Str Strinsert_charp(Str, int, const char*);
Str Strinsert_charp_n(Str s, int pos, const char* p, int n);
Str Strdelete(Str, int, int);
Str Strtruncate(Str, int);
Str Strlower(Str);
Str Strupper(Str);
Str Strchop(Str);
Str Strshrink(Str, int);
Str Strremovefirstspaces(Str);
Str Strremovetrailingspaces(Str);
Str Stralign_left(Str, int);
Str Stralign_right(Str, int);
Str Stralign_center(Str, int);

#ifdef GNUC
#define FORMAT_PRINTF(fmt, arg) attribute((format(printf, fmt, arg)))
#else
#define FORMAT_PRINTF(fmt, arg)
#endif
Str Sprintf(const char* fmt, ...) FORMAT_PRINTF(1, 2);

Str Strfgets(FILE*);
Str Strfgetall(FILE*);

Str Strgrow(Str s);

inline static Str Strcat_char(Str x, char y) { return Strinsert_char(x, (x)->length, y); }
inline static int Strcmp(Str x, Str y)
{
    return strcmp((x)->ptr, (y)->ptr);
}
inline static int Strcmp_charp(Str x, const char* y)
{
    return strcmp((x)->ptr, (y));
}
inline static int Strcasecmp(Str x, Str y)
{
    return strcasecmp((x)->ptr, (y)->ptr);
}
inline static int Strcasecmp_charp(Str x, const char* y)
{
    return strcasecmp((x)->ptr, (y));
}
inline static int Strncasecmp_charp(Str x, const char* y, int n)
{
    return strncasecmp((x)->ptr, (y), (n));
}
inline static char Strlastchar(Str s)
{
    return ((s)->length > 0 ? (s)->ptr[(s)->length - 1] : '\0');
}
inline static void Strshrinkfirst(Str s, int n)
{
    Strdelete((s), 0, (n));
}
inline static void Strfputs(Str s, FILE* f)
{
    fwrite((s)->ptr, 1, (s)->length, (f));
}

char* allocStr(const char* s, int len);
