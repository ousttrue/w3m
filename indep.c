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

char *HTML_QUOTE_MAP[] = {
    NULL, "&amp;", "&lt;", "&gt;", "&quot;", "&apos;", NULL, NULL,
};

int64_t strtoclen(const char *s) {
#ifdef HAVE_STRTOLL
  return strtoll(s, NULL, 10);
#elif defined(HAVE_STRTOQ)
  return strtoq(s, NULL, 10);
#elif defined(HAVE_ATOLL)
  return atoll(s);
#elif defined(HAVE_ATOQ)
  return atoq(s);
#else
  return atoi(s);
#endif
}

int strCmp(const void *s1, const void *s2) {
  return strcmp(*(const char **)s1, *(const char **)s2);
}

char *currentdir() {
  char *path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
  path = NewAtom_N(char, MAXPATHLEN);
  getcwd(path, MAXPATHLEN);
#else
  path = getcwd(NULL, 0);
#endif
#else /* not HAVE_GETCWD */
#ifdef HAVE_GETWD
  path = NewAtom_N(char, 1024);
  getwd(path);
#else  /* not HAVE_GETWD */
  FILE *f;
  char *p;
  path = NewAtom_N(char, 1024);
  f = popen("pwd", "r");
  fgets(path, 1024, f);
  pclose(f);
  for (p = path; *p; p++)
    if (*p == '\n') {
      *p = '\0';
      break;
    }
#endif /* not HAVE_GETWD */
#endif /* not HAVE_GETCWD */
  return path;
}

char *cleanupName(char *name) {
  char *buf, *p, *q;

  buf = allocStr(name, -1);
  p = buf;
  q = name;
  while (*q != '\0') {
    if (strncmp(p, "/../", 4) == 0) { /* foo/bar/../FOO */
      if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
        /* ../../       */
        p += 3;
        q += 3;
      } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
        /* ../../../    */
        p += 3;
        q += 3;
      } else {
        while (p != buf && *--p != '/')
          ; /* ->foo/FOO */
        *p = '\0';
        q += 3;
        strcat(buf, q);
      }
    } else if (strcmp(p, "/..") == 0) { /* foo/bar/..   */
      if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
        /* ../..        */
      } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
        /* ../../..     */
      } else {
        while (p != buf && *--p != '/')
          ; /* ->foo/ */
        *++p = '\0';
      }
      break;
    } else if (strncmp(p, "/./", 3) == 0) { /* foo/./bar */
      *p = '\0';                            /* -> foo/bar           */
      q += 2;
      strcat(buf, q);
    } else if (strcmp(p, "/.") == 0) { /* foo/. */
      *++p = '\0';                     /* -> foo/              */
      break;
    } else if (strncmp(p, "//", 2) == 0) { /* foo//bar */
      /* -> foo/bar           */
      *p = '\0';
      q++;
      strcat(buf, q);
    } else {
      p++;
      q++;
    }
  }
  return buf;
}

// #ifndef HAVE_STRCHR
// char *strchr(const char *s, int c) {
//   while (*s) {
//     if ((unsigned char)*s == c)
//       return (char *)s;
//     s++;
//   }
//   return NULL;
// }
// #endif /* not HAVE_STRCHR */

int strmatchlen(const char *s1, const char *s2, int maxlen) {
  int i;

  /* To allow the maxlen to be negatie (infinity),
   * compare by "!=" instead of "<=". */
  for (i = 0; i != maxlen; ++i) {
    if (!s1[i] || !s2[i] || s1[i] != s2[i])
      break;
  }
  return i;
}

char *remove_space(char *str) {
  char *p, *q;

  for (p = str; *p && IS_SPACE(*p); p++)
    ;
  for (q = p; *q; q++)
    ;
  for (; q > p && IS_SPACE(*(q - 1)); q--)
    ;
  if (*q != '\0')
    return Strnew_charp_n(p, q - p)->ptr;
  return p;
}

int non_null(char *s) {
  if (s == NULL)
    return false;
  while (*s) {
    if (!IS_SPACE(*s))
      return true;
    s++;
  }
  return false;
}

void cleanup_line(Str s, enum CLEANUP_LINE_MODE mode) {
  if (s->length >= 2 && s->ptr[s->length - 2] == '\r' &&
      s->ptr[s->length - 1] == '\n') {
    Strshrink(s, 2);
    Strcat_char(s, '\n');
  } else if (Strlastchar(s) == '\r')
    s->ptr[s->length - 1] = '\n';
  else if (Strlastchar(s) != '\n')
    Strcat_char(s, '\n');
  if (mode != PAGER_MODE) {
    int i;
    for (i = 0; i < s->length; i++) {
      if (s->ptr[i] == '\0')
        s->ptr[i] = ' ';
    }
  }
}

/*
 * convert line
 */
Str convertLine(Str line, enum CLEANUP_LINE_MODE mode) {
  if (mode != RAW_MODE)
    cleanup_line(line, mode);
  return line;
}

const char *html_quote(const char *str) {
  Str tmp = NULL;
  for (auto p = str; *p; p++) {
    auto q = html_quote_char(*p);
    if (q) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_charp(tmp, q);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

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

char *file_quote(char *str) {
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

char *file_unquote(char *str) {
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

const char *shell_quote(const char *str) {
  Str tmp = NULL;
  for (auto p = str; *p; p++) {
    if (is_shell_unsafe(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_char(tmp, '\\');
      Strcat_char(tmp, *p);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

void *xrealloc(void *ptr, size_t size) {
  void *newptr = realloc(ptr, size);
  if (newptr == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(-1);
  }
  return newptr;
}

static char *w3m_dir(const char *name, char *dft) {
#ifdef USE_PATH_ENVVAR
  char *value = getenv(name);
  return value ? value : dft;
#else
  return dft;
#endif
}

char *w3m_auxbin_dir() { return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR); }

char *w3m_lib_dir() {
  /* FIXME: use W3M_CGIBIN_DIR? */
  return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

char *w3m_etc_dir() { return w3m_dir("W3M_ETC_DIR", ETC_DIR); }

char *w3m_conf_dir() { return w3m_dir("W3M_CONF_DIR", CONF_DIR); }

char *w3m_help_dir() { return w3m_dir("W3M_HELP_DIR", HELP_DIR); }

const char *expandPath(const char *name) {
  if (!name) {
    return nullptr;
  }

  if (name[0] != '~') {
    return name;
  }

  auto p = name + 1;
  if (*p != '/' && *p != '\0') {
    return name;
  }

  /* ~/dir... or ~ */
  auto extpath = Strnew_charp(getenv("HOME"));
  if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
    p++;
  Strcat_charp(extpath, p);
  return extpath->ptr;
}
