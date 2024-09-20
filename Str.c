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
#include <stdio.h>
#include <stdlib.h>
#include <gc/gc.h>
#include <stdarg.h>
#include <string.h>
#ifdef __EMX__			/* or include "fm.h" for HAVE_BCOPY? */
#include <strings.h>
#endif
#include "Str.h"
#include "myctype.h"

#define INITIAL_STR_SIZE 32
#define STR_SIZE_MAX (STR_LEN_MAX + 1)

static Str Strgrow_n(Str s, int n);

Str
Strnew(void)
{
    return Strnew_size(INITIAL_STR_SIZE);
}

Str
Strnew_size(int n)
{
    Str x;

    if (!(x = GC_MALLOC(sizeof(struct _Str))))
	exit(1);

    if (n < 0 || n > STR_LEN_MAX)
	n = STR_SIZE_MAX;
    else if (n < INITIAL_STR_SIZE)
	n = INITIAL_STR_SIZE;
    if (!(x->ptr = GC_MALLOC_ATOMIC(n)))
	exit(1);
    x->area_size = n;
    x->length = 0;
    x->ptr[x->length] = '\0';
    return x;
}

Str
Strnew_charp(const char *p)
{
    return p ? Strnew_charp_n(p, strlen(p)) : Strnew();
}

Str
Strnew_m_charp(const char *p, ...)
{
    va_list ap;
    Str r = Strnew();

    va_start(ap, p);
    while (p != NULL) {
	Strcat_charp(r, p);
	p = va_arg(ap, char *);
    }
    va_end(ap);
    return r;
}

Str
Strnew_charp_n(const char *p, int n)
{
    Str x;

    x = Strnew_size(n);
    if (p)
	Strcopy_charp_n(x, p, n);
    return x;
}

Str
Strdup(Str s)
{
    Str n = Strnew_size(s->length);
    Strcopy(n, s);
    return n;
}

Str
Strclear(Str s)
{
    s->length = 0;
    s->ptr[s->length] = '\0';
    return s;
}

void
Strfree(Str x)
{
    if (!x) return;
    GC_free(x->ptr);
    GC_free(x);
}

Str
Strcopy(Str dst, Str src)
{
    return Strcopy_charp_n(dst, src->ptr, src->length);
}

Str
Strcopy_charp(Str x, const char *y)
{
    int len;

    if (!y)
	return Strtruncate(x, 0);

    len = strlen(y);
    return Strcopy_charp_n(x, y, len);
}

Str
Strcopy_charp_n(Str x, const char *y, int n)
{
    if (!y)
	return Strtruncate(x, 0);

    if (n > STR_LEN_MAX)
	n = STR_LEN_MAX;
    if (x->area_size <= n)
	Strgrow_n(x, n);
    memmove(x->ptr, y, n);
    x->length = n;
    x->ptr[x->length] = '\0';
    return x;
}

Str
Strcat_charp_n(Str x, const char *y, int n)
{
    int newlen;

    if (!y || !n || x->length == STR_LEN_MAX)
	return x;

    if (n < 0)
	n = strlen(y);
    newlen = x->length + n;
    if (newlen > STR_LEN_MAX) {
	newlen = STR_LEN_MAX;
	n = STR_LEN_MAX - x->length;
    }

    if (newlen >= x->area_size)
	Strgrow_n(x, newlen);
    memmove(&x->ptr[x->length], y, n);
    x->length += n;
    x->ptr[x->length] = '\0';
    return x;
}

Str
Strcat(Str x, Str y)
{
    return Strcat_charp_n(x, y->ptr, y->length);
}

Str
Strcat_charp(Str x, const char *y)
{
    if (!y)
	return x;
    return Strcat_charp_n(x, y, strlen(y));
}

Str
Strcat_m_charp(Str x, ...)
{
    va_list ap;
    char *p;

    va_start(ap, x);
    while ((p = va_arg(ap, char *)) != NULL)
	 Strcat_charp_n(x, p, strlen(p));
    va_end(ap);
    return x;
}

Str
Strgrow_n(Str x, int n)
{
    if (n < 0)
	n = STR_SIZE_MAX;
    else
	n = (n >= STR_SIZE_MAX) ? STR_SIZE_MAX : n + 1;

    if (x->area_size >= n)
	return x;

    if (!(x->ptr = GC_REALLOC(x->ptr, n)))
	exit(1);
    x->area_size = n;
    return x;
}

Str
Strgrow(Str x)
{
    int newlen, addlen;

    if (x->area_size < 8192)
	addlen = x->area_size;
    else
	addlen = x->area_size / 2;
    if (addlen < INITIAL_STR_SIZE)
	addlen = INITIAL_STR_SIZE;
    newlen = x->area_size + addlen;
    if (newlen <= 0 || newlen > STR_SIZE_MAX) {
	newlen = STR_SIZE_MAX;
	if (x->length + 1 >= newlen)
	    x->length = newlen - 2;
    }
    return Strgrow_n(x, newlen - 1);
}

Str
Strsubstr(Str s, int beg, int len)
{
    Str new_s;
    int i;

    new_s = Strnew();
    if (beg >= s->length)
	return new_s;
    for (i = 0; i < len && beg + i < s->length; i++)
	Strcat_char(new_s, s->ptr[beg + i]);
    return new_s;
}

Str
Strlower(Str s)
{
    int i;
    for (i = 0; i < s->length; i++)
	s->ptr[i] = TOLOWER(s->ptr[i]);
    return s;
}

Str
Strupper(Str s)
{
    int i;
    for (i = 0; i < s->length; i++)
	s->ptr[i] = TOUPPER(s->ptr[i]);
    return s;
}

Str
Strchop(Str s)
{
    while (s->length > 0 &&
	   (s->ptr[s->length - 1] == '\n' || s->ptr[s->length - 1] == '\r')) {
	s->length--;
    }
    s->ptr[s->length] = '\0';
    return s;
}

Str
Strinsert_char(Str s, int pos, char c)
{
    int i;
    if (pos < 0 || s->length < pos || s->length == STR_LEN_MAX)
	return s;
    if (s->length + 1 >= s->area_size)
	Strgrow(s);
    for (i = s->length; i > pos; i--)
	s->ptr[i] = s->ptr[i - 1];
    s->length++;
    s->ptr[s->length] = '\0';
    s->ptr[pos] = c;
    return s;
}

Str
Strinsert_charp(Str s, int pos, const char *p)
{
    while (*p)
	Strinsert_char(s, pos++, *(p++));
    return s;
}

Str
Strdelete(Str s, int pos, int n)
{
    int i;
    if (pos < 0 || s->length < pos)
	return s;
    if (n < 0)
	n = STR_LEN_MAX - pos;
    if (s->length <= pos + n) {
	s->length = pos;
	s->ptr[s->length] = '\0';
	return s;
    }
    for (i = pos; i < s->length - n; i++)
	s->ptr[i] = s->ptr[i + n];
    s->length = i;
    s->ptr[s->length] = '\0';
    return s;
}

Str
Strtruncate(Str s, int pos)
{
    if (pos < 0 || s->length < pos)
	return s;
    s->length = pos;
    s->ptr[s->length] = '\0';
    return s;
}

Str
Strshrink(Str s, int n)
{
    s->length = (n >= s->length) ? 0 : (s->length - n);
    s->ptr[s->length] = '\0';
    return s;
}

Str
Strremovefirstspaces(Str s)
{
    int i;

    for (i = 0; i < s->length && IS_SPACE(s->ptr[i]); i++) ;
    if (i == 0)
	return s;
    return Strdelete(s, 0, i);
}

Str
Strremovetrailingspaces(Str s)
{
    int i;

    for (i = s->length - 1; i >= 0 && IS_SPACE(s->ptr[i]); i--) ;
    s->length = i + 1;
    s->ptr[s->length] = '\0';
    return s;
}

Str
Stralign_left(Str s, int width)
{
    Str n;
    int i;

    if (s->length >= width)
	return Strdup(s);
    n = Strnew_size(width);
    Strcopy(n, s);
    for (i = s->length; i < width; i++)
	Strcat_char(n, ' ');
    return n;
}

Str
Stralign_right(Str s, int width)
{
    Str n;
    int i;

    if (s->length >= width)
	return Strdup(s);
    n = Strnew_size(width);
    for (i = s->length; i < width; i++)
	Strcat_char(n, ' ');
    Strcat(n, s);
    return n;
}

Str
Stralign_center(Str s, int width)
{
    Str n;
    int i, w;

    if (s->length >= width)
	return Strdup(s);
    n = Strnew_size(width);
    w = (width - s->length) / 2;
    for (i = 0; i < w; i++)
	Strcat_char(n, ' ');
    Strcat(n, s);
    for (i = w + s->length; i < width; i++)
	Strcat_char(n, ' ');
    return n;
}

#define SP_NORMAL 0
#define SP_PREC   1
#define SP_PREC2  2

Str
Sprintf(const char *fmt, ...)
{
    int len = 0;
    int status = SP_NORMAL;
    int p = 0;
    const char *f;
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
	    }
	    else
		len++;
	    break;
	case SP_PREC:
	    if (IS_ALPHA(*f)) {
		/* conversion char. */
		int vi;
		char *vs;

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
		    va_arg(ap, int);
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
		    va_arg(ap, int);
		    break;
		case 's':
		    vs = va_arg(ap, char *);
		    vi = strlen(vs);
		    len += (p > vi) ? p : vi;
		    break;
		case 'p':
		    va_arg(ap, void *);
		    len += 10;
		    break;
		case 'n':
		    va_arg(ap, void *);
		    break;
		}
		status = SP_NORMAL;
	    }
	    else if (IS_DIGIT(*f))
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

Str
Strfgets(FILE * f)
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

Str
Strfgetall(FILE * f)
{
    Str s = Strnew();
    int c;
    while ((c = fgetc(f)) != EOF) {
	Strcat_char(s, c);
    }
    return s;
}
