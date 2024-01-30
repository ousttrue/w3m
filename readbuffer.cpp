#include "readbuffer.h"
#include "Str.h"
#include "myctype.h"
#include "ctrlcode.h"

static char roman_num1[] = {
    'i', 'x', 'c', 'm', '*',
};
static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str *romanNum2(int l, int n) {
  Str *s = Strnew();

  switch (n) {
  case 1:
  case 2:
  case 3:
    for (; n > 0; n--)
      Strcat_char(s, roman_num1[l]);
    break;
  case 4:
    Strcat_char(s, roman_num1[l]);
    Strcat_char(s, roman_num5[l]);
    break;
  case 5:
  case 6:
  case 7:
  case 8:
    Strcat_char(s, roman_num5[l]);
    for (n -= 5; n > 0; n--)
      Strcat_char(s, roman_num1[l]);
    break;
  case 9:
    Strcat_char(s, roman_num1[l]);
    Strcat_char(s, roman_num1[l + 1]);
    break;
  }
  return s;
}

Str *romanNumeral(int n) {
  Str *r = Strnew();

  if (n <= 0)
    return r;
  if (n >= 4000) {
    Strcat_charp(r, "**");
    return r;
  }
  Strcat(r, romanNum2(3, n / 1000));
  Strcat(r, romanNum2(2, (n % 1000) / 100));
  Strcat(r, romanNum2(1, (n % 100) / 10));
  Strcat(r, romanNum2(0, n % 10));

  return r;
}

Str *romanAlphabet(int n) {
  Str *r = Strnew();
  int l;
  char buf[14];

  if (n <= 0)
    return r;

  l = 0;
  while (n) {
    buf[l++] = 'a' + (n - 1) % 26;
    n = (n - 1) / 26;
  }
  l--;
  for (; l >= 0; l--)
    Strcat_char(r, buf[l]);

  return r;
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

int read_token(Str *buf, char **instr, int *status, int pre, int append) {
  char *p;
  int prev_status;

  if (!append)
    Strclear(buf);
  if (**instr == '\0')
    return 0;
  for (p = *instr; *p; p++) {
    /* Drop Unicode soft hyphen */
    if (*(unsigned char *)p == 0210 && *(unsigned char *)(p + 1) == 0200 &&
        *(unsigned char *)(p + 2) == 0201 &&
        *(unsigned char *)(p + 3) == 0255) {
      p += 3;
      continue;
    }

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

Str *correct_irrtag(int status) {
  char c;
  Str *tmp = Strnew();

  while (status != R_ST_NORMAL) {
    switch (status) {
    case R_ST_CMNT:   /* required "-->" */
    case R_ST_NCMNT1: /* required "->" */
      c = '-';
      break;
    case R_ST_NCMNT2:
    case R_ST_NCMNT3:
    case R_ST_IRRTAG:
    case R_ST_CMNT1:
    case R_ST_CMNT2:
    case R_ST_TAG:
    case R_ST_TAG0:
    case R_ST_EQL: /* required ">" */
    case R_ST_VALUE:
      c = '>';
      break;
    case R_ST_QUOTE:
      c = '\'';
      break;
    case R_ST_DQUOTE:
      c = '"';
      break;
    case R_ST_AMP:
      c = ';';
      break;
    default:
      return tmp;
    }
    next_status(c, &status);
    Strcat_char(tmp, c);
  }
  return tmp;
}
