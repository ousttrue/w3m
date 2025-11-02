/* $Id: Str.c,v 1.8 2002/12/24 17:20:46 ukai Exp $ */
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
#include "alloc.h"
#include <gc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "myctype.h"

#ifdef STR_DEBUG
/* This is obsolete, because "Str" can handle a '\0' character now. */
#define STR_LENGTH_CHECK(x)                                                       \
    if (((x)->ptr == 0 && (x)->length != 0) || (strlen((x)->ptr) != (x)->length)) \
        abort();
#else /* not STR_DEBUG */
#define STR_LENGTH_CHECK(x)
#endif /* not STR_DEBUG */

#define SP_NORMAL 0
#define SP_PREC 1
#define SP_PREC2 2

Str Sprintf(char* fmt, ...)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;
    char* f;
    Str s;
    va_list ap;

    va_start(ap, fmt);
    for (f = fmt; *f; f++) {
    redo:
        switch (status) {
        case SP_NORMAL:
            if (*f == '%') {
                status = SP_PREC;
                p = 0;
            } else
                len++;
            break;
        case SP_PREC:
            if (IS_ALPHA(*f)) {
                /* conversion char. */
                int vi;
                char* vs;

                switch (*f) {
                case 'l':
                case 'h':
                case 'L':
                case 'w':
                    continue;
                case 'd':
                case 'i':
                case 'o':
                case 'x':
                case 'X':
                case 'u':
                    vi = va_arg(ap, int);
                    len += (p > 0) ? p : 10;
                    break;
                case 'f':
                case 'g':
                case 'e':
                case 'G':
                case 'E':
                    va_arg(ap, double);
                    len += (p > 0) ? p : 15;
                    break;
                case 'c':
                    len += 1;
                    vi = va_arg(ap, int);
                    break;
                case 's':
                    vs = va_arg(ap, char*);
                    vi = strlen(vs);
                    len += (p > vi) ? p : vi;
                    break;
                case 'p':
                    va_arg(ap, void*);
                    len += 10;
                    break;
                case 'n':
                    va_arg(ap, void*);
                    break;
                }
                status = SP_NORMAL;
            } else if (IS_DIGIT(*f))
                p = p * 10 + *f - '0';
            else if (*f == '.')
                status = SP_PREC2;
            else if (*f == '%') {
                status = SP_NORMAL;
                len++;
            }
            break;
        case SP_PREC2:
            if (IS_ALPHA(*f)) {
                status = SP_PREC;
                goto redo;
            }
            break;
        }
    }
    va_end(ap);
    s = Strnew_size(len * 2);
    va_start(ap, fmt);
    vsprintf(s->ptr, fmt, ap);
    va_end(ap);
    s->length = strlen(s->ptr);
    if (s->length > len * 2) {
        fprintf(stderr, "Sprintf: string too long\n");
        exit(1);
    }
    return s;
}

Str Strfgets(FILE* f)
{
    Str s = Strnew();
    int c;
    while ((c = fgetc(f)) != EOF) {
        Strcat_char(s, c);
        if (c == '\n')
            break;
    }
    return s;
}

Str Strfgetall(FILE* f)
{
    Str s = Strnew();
    int c;
    while ((c = fgetc(f)) != EOF) {
        Strcat_char(s, c);
    }
    return s;
}

int Strcmp(Str x, Str y) { return strcmp(x->ptr, y->ptr); }
int Strcmp_charp(Str x, const char* y) { return strcmp(x->ptr, y); }
int Strncmp(Str x, Str y, size_t n) { return strncmp(x->ptr, y->ptr, n); }
int Strncmp_charp(Str x, const char* y, size_t n) { return strncmp(x->ptr, y, n); }
int Strcasecmp(Str x, Str y) { return strcasecmp(x->ptr, y->ptr); }
int Strcasecmp_charp(Str x, const char* y) { return strcasecmp(x->ptr, y); }
int Strncasecmp(Str x, Str y, size_t n) { return strncasecmp(x->ptr, y->ptr, n); }
int Strncasecmp_charp(Str x, const char* y, size_t n) { return strncasecmp(x->ptr, y, n); }

char Strlastchar(Str s) { return s->length > 0 ? s->ptr[s->length - 1] : '\0'; }
int Strfputs(Str s, FILE* f) { return fwrite((s)->ptr, 1, (s)->length, (f)); }
