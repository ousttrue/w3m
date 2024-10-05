#include "html/html_parser.h"
#include "buffer/line.h"
#include "entity.h"
#include "alloc.h"
#include "text.h"
#include "myctype.h"
#include "hash.h"
#include "utf8.h"
#include <string.h>
#include <strings.h>

uint32_t getescapechar(const char **str) {
  auto p = *str;
  bool strict_entity = true;

  if (*p == '&')
    p++;
  if (*p == '#') {
    p++;
    if (*p == 'x' || *p == 'X') {
      p++;
      if (!IS_XDIGIT(*p)) {
        *str = p;
        return -1;
      }
      uint32_t dummy = 0;
      for (dummy = GET_MYCDIGIT(*p), p++; IS_XDIGIT(*p); p++)
        dummy = dummy * 0x10 + GET_MYCDIGIT(*p);
      if (*p == ';')
        p++;
      *str = p;
      return dummy;
    } else {
      if (!IS_DIGIT(*p)) {
        *str = p;
        return -1;
      }
      uint32_t dummy = 0;
      for (dummy = GET_MYCDIGIT(*p), p++; IS_DIGIT(*p); p++)
        dummy = dummy * 10 + GET_MYCDIGIT(*p);
      if (*p == ';')
        p++;
      *str = p;
      return dummy;
    }
  }
  if (!IS_ALPHA(*p)) {
    *str = p;
    return 0;
  }
  auto q = p;
  for (p++; IS_ALNUM(*p); p++)
    ;
  q = allocStr(q, p - q);
  if (strcasestr("lt gt amp quot apos nbsp", q) && *p != '=') {
    /* a character entity MUST be terminated with ";". However,
     * there's MANY web pages which uses &lt , &gt or something
     * like them as &lt;, &gt;, etc. Therefore, we treat the most
     * popular character entities (including &#xxxx;) without
     * the last ";" as character entities. If the trailing character
     * is "=", it must be a part of query in an URL. So &lt=, &gt=, etc.
     * are not regarded as character entities.
     */
    strict_entity = false;
  }
  if (*p == ';')
    p++;
  else if (strict_entity) {
    *str = p;
    return 0;
  }
  *str = p;
  return getHash_si(&entity, q, -1);
}

const char *getescapecmd(const char **s) {
  auto save = *s;

  auto ch = getescapechar(s);
  if (ch > 0) {
    // return conv_entity(ch);
    uint8_t utf8[4];
    int utf8_len = utf8sequence_from_codepoint(ch, utf8);
    if (utf8_len) {
      return allocStr((char *)utf8, utf8_len);
    } else {
      return "�";
    }
  }

  Str tmp;
  if (*save != '&')
    tmp = Strnew_charp("&");
  else
    tmp = Strnew();
  Strcat_charp_n(tmp, save, *s - save);
  return tmp->ptr;
}

const char *html_unquote(const char *str) {
  Str tmp = NULL;
  for (auto p = str; *p;) {
    if (*p == '&') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      auto q = getescapecmd(&p);
      Strcat_charp(tmp, q);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
      p++;
    }
  }
  return tmp ? tmp->ptr : str;
}

int next_status(char c, int *status) {
  switch (*status) {
  case R_ST_NORMAL:
    if (c == '<') {
      *status = R_ST_TAG0;
      return 0;
    } else if (c == '&') {
      *status = R_ST_AMP;
      return 1;
    } else
      return 1;
    break;
  case R_ST_TAG0:
    if (c == '!') {
      *status = R_ST_CMNT1;
      return 0;
    }
    *status = R_ST_TAG;
    /* continues to next case */
  case R_ST_TAG:
    if (c == '>')
      *status = R_ST_NORMAL;
    else if (c == '=')
      *status = R_ST_EQL;
    return 0;
  case R_ST_EQL:
    if (c == '"')
      *status = R_ST_DQUOTE;
    else if (c == '\'')
      *status = R_ST_QUOTE;
    else if (IS_SPACE(c))
      *status = R_ST_EQL;
    else if (c == '>')
      *status = R_ST_NORMAL;
    else
      *status = R_ST_VALUE;
    return 0;
  case R_ST_QUOTE:
    if (c == '\'')
      *status = R_ST_TAG;
    return 0;
  case R_ST_DQUOTE:
    if (c == '"')
      *status = R_ST_TAG;
    return 0;
  case R_ST_VALUE:
    if (c == '>')
      *status = R_ST_NORMAL;
    else if (IS_SPACE(c))
      *status = R_ST_TAG;
    return 0;
  case R_ST_AMP:
    if (c == ';') {
      *status = R_ST_NORMAL;
      return 0;
    } else if (c != '#' && !IS_ALNUM(c) && c != '_') {
      /* something's wrong! */
      *status = R_ST_NORMAL;
      return 0;
    } else
      return 0;
  case R_ST_CMNT1:
    switch (c) {
    case '-':
      *status = R_ST_CMNT2;
      break;
    case '>':
      *status = R_ST_NORMAL;
      break;
    case 'D':
    case 'd':
      /* could be a !doctype */
      *status = R_ST_TAG;
      break;
    default:
      *status = R_ST_IRRTAG;
    }
    return 0;
  case R_ST_CMNT2:
    switch (c) {
    case '-':
      *status = R_ST_CMNT;
      break;
    case '>':
      *status = R_ST_NORMAL;
      break;
    default:
      *status = R_ST_IRRTAG;
    }
    return 0;
  case R_ST_CMNT:
    if (c == '-')
      *status = R_ST_NCMNT1;
    return 0;
  case R_ST_NCMNT1:
    if (c == '-')
      *status = R_ST_NCMNT2;
    else
      *status = R_ST_CMNT;
    return 0;
  case R_ST_NCMNT2:
    switch (c) {
    case '>':
      *status = R_ST_NORMAL;
      break;
    case '-':
      *status = R_ST_NCMNT2;
      break;
    default:
      if (IS_SPACE(c))
        *status = R_ST_NCMNT3;
      else
        *status = R_ST_CMNT;
      break;
    }
    break;
  case R_ST_NCMNT3:
    switch (c) {
    case '>':
      *status = R_ST_NORMAL;
      break;
    case '-':
      *status = R_ST_NCMNT1;
      break;
    default:
      if (IS_SPACE(c))
        *status = R_ST_NCMNT3;
      else
        *status = R_ST_CMNT;
      break;
    }
    return 0;
  case R_ST_IRRTAG:
    if (c == '>')
      *status = R_ST_NORMAL;
    return 0;
  }
  /* notreached */
  return 0;
}

int read_token(Str buf, const char **instr, int *status, int pre, int append) {
  char *p;
  int prev_status;

  if (!append)
    Strclear(buf);
  if (**instr == '\0')
    return 0;
  for (p = *instr; *p; p++) {
    prev_status = *status;
    next_status(*p, status);
    switch (*status) {
    case R_ST_NORMAL:
      if (prev_status == R_ST_AMP && *p != ';') {
        p--;
        break;
      }
      if (prev_status == R_ST_NCMNT2 || prev_status == R_ST_NCMNT3 ||
          prev_status == R_ST_IRRTAG || prev_status == R_ST_CMNT1) {
        if (prev_status == R_ST_CMNT1 && !append && !pre)
          Strclear(buf);
        if (pre)
          Strcat_char(buf, *p);
        p++;
        goto proc_end;
      }
      Strcat_char(buf, (!pre && IS_SPACE(*p)) ? ' ' : *p);
      if (ST_IS_REAL_TAG(prev_status)) {
        *instr = p + 1;
        if (buf->length < 2 || buf->ptr[buf->length - 2] != '<' ||
            buf->ptr[buf->length - 1] != '>')
          return 1;
        Strshrink(buf, 2);
      }
      break;
    case R_ST_TAG0:
    case R_ST_TAG:
      if (prev_status == R_ST_NORMAL && p != *instr) {
        *instr = p;
        *status = prev_status;
        return 1;
      }
      if (*status == R_ST_TAG0 && !REALLY_THE_BEGINNING_OF_A_TAG(p)) {
        /* it seems that this '<' is not a beginning of a tag */
        /*
         * Strcat_charp(buf, "&lt;");
         */
        Strcat_char(buf, '<');
        *status = R_ST_NORMAL;
      } else
        Strcat_char(buf, *p);
      break;
    case R_ST_EQL:
    case R_ST_QUOTE:
    case R_ST_DQUOTE:
    case R_ST_VALUE:
    case R_ST_AMP:
      Strcat_char(buf, *p);
      break;
    case R_ST_CMNT:
    case R_ST_IRRTAG:
      if (pre)
        Strcat_char(buf, *p);
      else if (!append)
        Strclear(buf);
      break;
    case R_ST_CMNT1:
    case R_ST_CMNT2:
    case R_ST_NCMNT1:
    case R_ST_NCMNT2:
    case R_ST_NCMNT3:
      /* do nothing */
      if (pre)
        Strcat_char(buf, *p);
      break;
    }
  }
proc_end:
  *instr = p;
  return 1;
}

#define PUSH_TAG(str, n) Strcat_char(tagbuf, *str)
int visible_length_offset = 0;

int maximum_visible_length(char *str, int offset) {
  visible_length_offset = offset;
  return visible_length(str);
}

int maximum_visible_length_plain(char *str, int offset) {
  visible_length_offset = offset;
  return visible_length_plain(str);
}

int visible_length(char *str) {
  int len = 0, n, max_len = 0;
  int status = R_ST_NORMAL;
  int prev_status = status;
  Str tagbuf = Strnew();
  char *t, *r2;
  int amp_len = 0;

  while (*str) {
    prev_status = status;
    if (next_status(*str, &status)) {
      len++;
    }
    if (status == R_ST_TAG0) {
      Strclear(tagbuf);
      PUSH_TAG(str, n);
    } else if (status == R_ST_TAG || status == R_ST_DQUOTE ||
               status == R_ST_QUOTE || status == R_ST_EQL ||
               status == R_ST_VALUE) {
      PUSH_TAG(str, n);
    } else if (status == R_ST_AMP) {
      if (prev_status == R_ST_NORMAL) {
        Strclear(tagbuf);
        len--;
        amp_len = 0;
      } else {
        PUSH_TAG(str, n);
        amp_len++;
      }
    } else if (status == R_ST_NORMAL && prev_status == R_ST_AMP) {
      PUSH_TAG(str, n);
      r2 = tagbuf->ptr;
      t = getescapecmd(&r2);
      if (!*r2 && (*t == '\r' || *t == '\n')) {
        if (len > max_len)
          max_len = len;
        len = 0;
      } else
        len += utf8str_width((const uint8_t *)t) +
               utf8str_width((const uint8_t *)r2);
    } else if (status == R_ST_NORMAL && ST_IS_REAL_TAG(prev_status)) {
      ;
    } else if (*str == '\t') {
      len--;
      do {
        len++;
      } while ((visible_length_offset + len) % Tabstop != 0);
    } else if (*str == '\r' || *str == '\n') {
      len--;
      if (len > max_len)
        max_len = len;
      len = 0;
    }
    str++;
  }
  if (status == R_ST_AMP) {
    r2 = tagbuf->ptr;
    t = getescapecmd(&r2);
    if (*t != '\r' && *t != '\n')
      len += utf8str_width((const uint8_t *)t) +
             utf8str_width((const uint8_t *)r2);
  }
  return len > max_len ? len : max_len;
}

int visible_length_plain(char *str) {
  int len = 0, max_len = 0;

  while (*str) {
    if (*str == '\t') {
      do {
        len++;
      } while ((visible_length_offset + len) % Tabstop != 0);
      str++;
    } else if (*str == '\r' || *str == '\n') {
      if (len > max_len)
        max_len = len;
      len = 0;
      str++;
    } else {
      len++;
      str++;
    }
  }
  return len > max_len ? len : max_len;
}
