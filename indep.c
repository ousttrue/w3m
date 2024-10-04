#include "indep.h"
#include "Str.h"
#include "myctype.h"
#include "alloc.h"
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>

#define HAVE_STRTOLL 1
#define HAVE_ATOLL 1

static char xdigit[0x10] = "0123456789ABCDEF";

#define url_unquote_char(pstr)                                                 \
  ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2]))                        \
       ? (*(pstr) += 3,                                                        \
          (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1]))    \
       : -1)

const char *url_quote(const char *str) {
  Str tmp = NULL;
  char *p;

  for (p = str; *p; p++) {
    if (is_url_quote(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_char(tmp, '%');
      Strcat_char(tmp, xdigit[((unsigned char)*p >> 4) & 0xF]);
      Strcat_char(tmp, xdigit[(unsigned char)*p & 0xF]);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

const char *file_quote(const char *str) {
  Str tmp = NULL;
  char *p;
  char buf[4];

  for (p = str; *p; p++) {
    if (is_file_quote(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      sprintf(buf, "%%%02X", (unsigned char)*p);
      Strcat_charp(tmp, buf);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

const char *file_unquote(const char *str) {
  Str tmp = NULL;
  char *p, *q;
  int c;

  for (p = str; *p;) {
    if (*p == '%') {
      q = p;
      c = url_unquote_char(&q);
      if (c >= 0) {
        if (tmp == NULL)
          tmp = Strnew_charp_n(str, (int)(p - str));
        if (c != '\0' && c != '\n' && c != '\r')
          Strcat_char(tmp, (char)c);
        p = q;
        continue;
      }
    }
    if (tmp)
      Strcat_char(tmp, *p);
    p++;
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

Str Str_form_quote(Str x) {
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

Str Str_url_unquote(Str x, bool is_form, bool safe) {
  Str tmp = NULL;
  char *p = x->ptr, *ep = x->ptr + x->length, *q;
  int c;

  for (; p < ep;) {
    if (is_form && *p == '+') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      Strcat_char(tmp, ' ');
      p++;
      continue;
    } else if (*p == '%') {
      q = p;
      c = url_unquote_char(&q);
      if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c))) {
        if (tmp == NULL)
          tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
        Strcat_char(tmp, (char)c);
        p = q;
        continue;
      }
    }
    if (tmp)
      Strcat_char(tmp, *p);
    p++;
  }
  if (tmp)
    return tmp;
  return x;
}

void *xrealloc(void *ptr, size_t size) {
  void *newptr = realloc(ptr, size);
  if (newptr == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(-1);
  }
  return newptr;
}
