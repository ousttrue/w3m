#include "text.h"
#include "alloc.h"
#include "line.h"
#include "myctype.h"
#include "ctrlcode.h"
#include <math.h>

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

bool non_null(const char *s) {
  if (s == NULL)
    return false;
  while (*s) {
    if (!IS_SPACE(*s))
      return true;
    s++;
  }
  return false;
}

static bool is_period_char(const unsigned char *ch) {
  switch (*ch) {
  case ',':
  case '.':
  case ':':
  case ';':
  case '?':
  case '!':
  case ')':
  case ']':
  case '}':
  case '>':
    return 1;
  default:
    return 0;
  }
}

static bool is_beginning_char(const unsigned char *ch) {
  switch (*ch) {
  case '(':
  case '[':
  case '{':
  case '`':
  case '<':
    return 1;
  default:
    return 0;
  }
}

static bool is_word_char(const unsigned char *ch) {
  Lineprop ctype = get_mctype(ch);

  if (ctype == PC_CTRL)
    return 0;

  if (IS_ALNUM(*ch))
    return 1;

  switch (*ch) {
  case ',':
  case '.':
  case ':':
  case '\"': /* " */
  case '\'':
  case '$':
  case '%':
  case '*':
  case '+':
  case '-':
  case '@':
  case '~':
  case '_':
    return 1;
  }
  if (*ch == TIMES_CODE || *ch == DIVIDE_CODE || *ch == ANSP_CODE)
    return 0;
  if (*ch >= AGRAVE_CODE || *ch == NBSP_CODE)
    return 1;
  return 0;
}

bool is_boundary(const unsigned char *ch1, const unsigned char *ch2) {
  if (!*ch1 || !*ch2)
    return 1;

  if (*ch1 == ' ' && *ch2 == ' ')
    return 0;

  if (*ch1 != ' ' && is_period_char(ch2))
    return 0;

  if (*ch2 != ' ' && is_beginning_char(ch1))
    return 0;

  if (is_word_char(ch1) && is_word_char(ch2))
    return 0;

  return 1;
}

static char *_size_unit[] = {"b",  "kb", "Mb", "Gb", "Tb", "Pb",
                             "Eb", "Zb", "Bb", "Yb", NULL};

const char *convert_size(int64_t size, int usefloat) {
  float csize;
  int sizepos = 0;
  char **sizes = _size_unit;

  csize = (float)size;
  while (csize >= 999.495 && sizes[sizepos + 1]) {
    csize = csize / 1024.0;
    sizepos++;
  }
  return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
                 floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
      ->ptr;
}

const char *convert_size2(int64_t size1, int64_t size2, int usefloat) {
  char **sizes = _size_unit;
  float csize, factor = 1;
  int sizepos = 0;

  csize = (float)((size1 > size2) ? size1 : size2);
  while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
    factor *= 1024.0;
    sizepos++;
  }
  return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
                 floor(size1 / factor * 100.0 + 0.5) / 100.0,
                 floor(size2 / factor * 100.0 + 0.5) / 100.0, sizes[sizepos])
      ->ptr;
}

const char *remove_space(const char *str) {
  const char *p;
  for (p = str; *p && IS_SPACE(*p); p++)
    ;
  const char *q;
  for (q = p; *q; q++)
    ;
  for (; q > p && IS_SPACE(*(q - 1)); q--)
    ;
  if (*q != '\0')
    return Strnew_charp_n(p, q - p)->ptr;
  return p;
}

Str unescape_spaces(Str s) {
  if (s == NULL)
    return s;

  Str tmp = NULL;
  for (auto p = s->ptr; *p; p++) {
    if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp;
  return s;
}

const char *cleanupName(const char *name) {
  char *buf = allocStr(name, -1);
  auto p = buf;
  auto q = name;
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

const char *getWord(const char **str) {
  auto p = *str;
  SKIP_BLANKS(p);
  const char *s;
  for (s = p; *p && !IS_SPACE(*p) && *p != ';'; p++)
    ;
  *str = p;
  return Strnew_charp_n(s, p - s)->ptr;
}

const char *getQWord(const char **str) {
  Str tmp = Strnew();
  int in_q = 0, in_dq = 0, esc = 0;

  const char *p = *str;
  SKIP_BLANKS(p);
  for (; *p; p++) {
    if (esc) {
      if (in_q) {
        if (*p != '\\' && *p != '\'') /* '..\\..', '..\'..' */
          Strcat_char(tmp, '\\');
      } else if (in_dq) {
        if (*p != '\\' && *p != '"') /* "..\\..", "..\".." */
          Strcat_char(tmp, '\\');
      } else {
        if (*p != '\\' && *p != '\'' && /* ..\\.., ..\'.. */
            *p != '"' && !IS_SPACE(*p)) /* ..\".., ..\.. */
          Strcat_char(tmp, '\\');
      }
      Strcat_char(tmp, *p);
      esc = 0;
    } else if (*p == '\\') {
      esc = 1;
    } else if (in_q) {
      if (*p == '\'')
        in_q = 0;
      else
        Strcat_char(tmp, *p);
    } else if (in_dq) {
      if (*p == '"')
        in_dq = 0;
      else
        Strcat_char(tmp, *p);
    } else if (*p == '\'') {
      in_q = 1;
    } else if (*p == '"') {
      in_dq = 1;
    } else if (IS_SPACE(*p) || *p == ';') {
      break;
    } else {
      Strcat_char(tmp, *p);
    }
  }
  *str = p;
  return tmp->ptr;
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

Str convertLine(Str line, enum CLEANUP_LINE_MODE mode) {
  if (mode != RAW_MODE)
    cleanup_line(line, mode);
  return line;
}
