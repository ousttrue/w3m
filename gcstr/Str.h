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
#include <stdio.h>
#include <limits.h>

#define STR_SIZE_MAX (INT_MAX / 32)

#define GCSTR_DETAIL
#ifdef GCSTR_DETAIL
struct Str {
    char* ptr;
    int length;
    int area_size;
};
#else
struct Str;
#endif

typedef struct Str* Str;

char* allocStr(const char* s, int len);
Str Strnew(void);
Str Strnew_size(int);
Str Strnew_charp(const char*);
Str Strnew_charp_n(const char*, int);
Str Strnew_m_charp(const char*, ...);
Str Strdup(Str);
void Strclear(Str);
void Strfree(Str);
void Strcopy(Str, Str);
void Strcopy_charp(Str, const char*);
void Strcopy_charp_n(Str, const char*, int);
void Strcat_charp_n(Str, const char*, int);
void Strcat(Str, Str);
void Strcat_charp(Str, const char*);
void Strcat_m_charp(Str, ...);
Str Strsubstr(Str, int, int);
void Strinsert_char(Str, int, char);
void Strinsert_charp(Str, int, const char*);
static inline void Strinsert(Str s, int n, Str p) { Strinsert_charp(s, n, p->ptr); }
void Strdelete(Str, int, int);
static inline void Strshrinkfirst(Str s, int n) { Strdelete(s, 0, n); }
void Strtruncate(Str, int);
void Strlower(Str);
void Strupper(Str);
void Strchop(Str);
void Strshrink(Str, int);
void Strremovefirstspaces(Str);
void Strremovetrailingspaces(Str);
Str Stralign_left(Str, int);
Str Stralign_right(Str, int);
Str Stralign_center(Str, int);

Str Sprintf(char* fmt, ...);

Str Strfgets(FILE*);
Str Strfgetall(FILE*);

void Strgrow(Str s);

Str Strcat_char(Str x, char y);
static inline void Strcatc(Str x, char y) { x->ptr[x->length++] = y; }
static inline void Strnulterm(Str x) { x->ptr[x->length] = 0; }

int Strcmp(Str x, Str y);
int Strcmp_charp(Str x, const char* y);
int Strncmp(Str x, Str y, size_t n);
int Strncmp_charp(Str x, const char* y, size_t n);
int Strcasecmp(Str x, Str y);
int Strcasecmp_charp(Str x, const char* y);
int Strncasecmp(Str x, Str y, size_t n);
int Strncasecmp_charp(Str x, const char* y, size_t n);

char Strlastchar(Str s);
int Strfputs(Str s, FILE* f);
