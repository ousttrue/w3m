/* vi: set sw=4 ts=8 ai sm noet : */
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
#ifndef GC_STR_H
#define GC_STR_H
#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifdef __EMX__
#define strcasecmp	stricmp
#define strncasecmp	strnicmp
#endif

typedef struct _Str {
    char *ptr;
    int length;
    int area_size;
} *Str;

Str Strnew(void);
Str Strnew_size(int);
Str Strnew_charp(const char *);
Str Strnew_charp_n(const char *, int);
Str Strnew_m_charp(const char *, ...);
Str Strdup(Str);
Str Strclear(Str);
void Strfree(Str);
Str Strcopy(Str dst, Str src);
Str Strcopy_charp(Str, const char *);
Str Strcopy_charp_n(Str, const char *, int);
Str Strcat_charp_n(Str, const char *, int);
Str Strcat(Str, Str);
Str Strcat_charp(Str, const char *);
Str Strcat_m_charp(Str, ...);
Str Strsubstr(Str, int, int);
Str Strinsert_char(Str, int, char);
Str Strinsert_charp(Str, int, const char *);
Str Strinsert_charp_n(Str s, int pos, const char *p, int n);
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

Str Sprintf(const char *fmt, ...);

Str Strfgets(FILE *);
Str Strfgetall(FILE *);

Str Strgrow(Str s);

#define STR_LEN_MAX (INT_MAX / 32 - 1)
#define Strcat_char(x,y)             Strinsert_char(x,(x)->length,y)
#define Strcmp(x,y)                  strcmp((x)->ptr,(y)->ptr)
#define Strcmp_charp(x,y)            strcmp((x)->ptr,(y))
#define Strcasecmp(x,y)              strcasecmp((x)->ptr,(y)->ptr)
#define Strcasecmp_charp(x,y)        strcasecmp((x)->ptr,(y))
#define Strncasecmp_charp(x,y,n)     strncasecmp((x)->ptr,(y),(n))

#define Strlastchar(s)               ((s)->length>0?(s)->ptr[(s)->length-1]:'\0')
#define Strshrinkfirst(s,n)          Strdelete((s),0,(n))
#define Strfputs(s,f)                fwrite((s)->ptr,1,(s)->length,(f))
#endif				/* not GC_STR_H */
