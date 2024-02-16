#include "readbuffer.h"
#include "etc.h"
#include "html_parser.h"
#include "html/form_item.h"
#include "app.h"
#include "buffer.h"
#include "line_layout.h"
#include "url_quote.h"
#include "quote.h"
#include "http_response.h"
#include "symbol.h"
#include "url_stream.h"
#include "w3m.h"
#include "hash.h"
#include "linklist.h"
#include "display.h"
#include "Str.h"
#include "myctype.h"
#include "ctrlcode.h"
#include "table.h"
#include "html.h"
#include "signal_util.h"
#include "form.h"
#include "textlist.h"
#include "terms.h"
#include "istream.h"
#include "htmltag.h"
#include "proto.h"
#include "alloc.h"
#include "funcname1.h"
#include "entity.h"
#include <math.h>
#include <algorithm>

#define ENABLE_REMOVE_TRAILINGSPACES

bool pseudoInlines = true;
bool ignore_null_img_alt = true;
double pixel_per_char = DEFAULT_PIXEL_PER_CHAR;
int pixel_per_char_i = DEFAULT_PIXEL_PER_CHAR;
int displayLinkNumber = false;
bool DisableCenter = false;
int Tabstop = 8;
int IndentIncr = 4;
bool DisplayBorders = false;
int displayInsDel = DISPLAY_INS_DEL_NORMAL;
bool view_unseenobject = true;
bool MetaRefresh = false;

#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */
#define REAL_WIDTH(w, limit)                                                   \
  (((w) >= 0) ? (int)((w) / pixel_per_char) : -(w) * (limit) / 100)

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG) {
  LONGJMP(AbortLoading, 1);
  SIGNAL_RETURN;
}

#define MAX_UL_LEVEL 9
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)
#define IMG_SYMBOL UL_SYMBOL(12)
#define HR_SYMBOL 26

static struct table *tables[MAX_TABLE];
static struct table_mode table_mode[MAX_TABLE];

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

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

int read_token(Str *buf, const char **instr, int *status, int pre, int append) {
  if (!append)
    Strclear(buf);
  if (**instr == '\0')
    return 0;

  int prev_status;
  const char *p;
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

static int is_period_char(unsigned char *ch) {
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

static int is_beginning_char(unsigned char *ch) {
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

static int is_word_char(unsigned char *ch) {
  Lineprop ctype = get_mctype((const char *)ch);

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

static int is_combining_char(const char *ch) {
  Lineprop ctype = get_mctype(ch);

  if (ctype & PC_WCHAR2)
    return 1;
  return 0;
}

int is_boundary(unsigned char *ch1, unsigned char *ch2) {
  if (!*ch1 || !*ch2)
    return 1;

  if (*ch1 == ' ' && *ch2 == ' ')
    return 0;

  if (*ch1 != ' ' && is_period_char(ch2))
    return 0;

  if (*ch2 != ' ' && is_beginning_char(ch1))
    return 0;

  if (is_combining_char((const char *)ch2))
    return 0;

  if (is_word_char(ch1) && is_word_char(ch2))
    return 0;

  return 1;
}

static void back_to_breakpoint(struct readbuffer *obuf) {
  obuf->flag = obuf->bp.flag;
  bcopy((void *)&obuf->bp.anchor, (void *)&obuf->anchor, sizeof(obuf->anchor));
  obuf->img_alt = obuf->bp.img_alt;
  obuf->input_alt = obuf->bp.input_alt;
  obuf->in_bold = obuf->bp.in_bold;
  obuf->in_italic = obuf->bp.in_italic;
  obuf->in_under = obuf->bp.in_under;
  obuf->in_strike = obuf->bp.in_strike;
  obuf->in_ins = obuf->bp.in_ins;
  obuf->prev_ctype = obuf->bp.prev_ctype;
  obuf->pos = obuf->bp.pos;
  obuf->top_margin = obuf->bp.top_margin;
  obuf->bottom_margin = obuf->bp.bottom_margin;
  if (obuf->flag & RB_NOBR)
    obuf->nobr_level = obuf->bp.nobr_level;
}

#define PUSH(c) push_char(obuf, obuf->flag &RB_SPECIAL, c)

Str *process_img(HtmlParser *parser, struct HtmlTag *tag, int width) {
  const char *p, *q, *r, *r2 = NULL, *s, *t;
  int w, i, nw, n;
  int pre_int = false, ext_pre_int = false;
  Str *tmp = Strnew();

  if (!parsedtag_get_value(tag, ATTR_SRC, &p))
    return tmp;
  p = Strnew(url_quote(remove_space(p)))->ptr;
  q = NULL;
  parsedtag_get_value(tag, ATTR_ALT, &q);
  if (!pseudoInlines && (q == NULL || (*q == '\0' && ignore_null_img_alt)))
    return tmp;
  t = q;
  parsedtag_get_value(tag, ATTR_TITLE, &t);
  w = -1;
  if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
    if (w < 0) {
      if (width > 0)
        w = (int)(-width * pixel_per_char * w / 100 + 0.5);
      else
        w = -1;
    }
  }
  i = -1;
  parsedtag_get_value(tag, ATTR_HEIGHT, &i);
  r = NULL;
  parsedtag_get_value(tag, ATTR_USEMAP, &r);
  if (tag->parsedtag_exists(ATTR_PRE_INT))
    ext_pre_int = true;

  tmp = Strnew_size(128);
  if (r) {
    Str *tmp2;
    r2 = strchr(r, '#');
    s = "<form_int method=internal action=map>";
    tmp2 = parser->process_form(HtmlTag::parse(&s, true));
    if (tmp2)
      Strcat(tmp, tmp2);
    Strcat(tmp, Sprintf("<input_alt fid=\"%d\" "
                        "type=hidden name=link value=\"",
                        parser->cur_form_id()));
    Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
    Strcat(tmp, Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                        "type=submit no_effect=true>",
                        parser->cur_hseq++, parser->cur_form_id()));
  }
  {
    if (w < 0)
      w = 12 * pixel_per_char;
    nw = w ? (int)((w - 1) / pixel_per_char + 1) : 1;
    if (r) {
      Strcat_charp(tmp, "<pre_int>");
      pre_int = true;
    }
    Strcat_charp(tmp, "<img_alt src=\"");
  }
  Strcat_charp(tmp, html_quote(p));
  Strcat_charp(tmp, "\"");
  if (t) {
    Strcat_charp(tmp, " title=\"");
    Strcat_charp(tmp, html_quote(t));
    Strcat_charp(tmp, "\"");
  }
  Strcat_charp(tmp, ">");
  if (q != NULL && *q == '\0' && ignore_null_img_alt)
    q = NULL;
  if (q != NULL) {
    n = get_strwidth(q);
    Strcat_charp(tmp, html_quote(q));
    goto img_end;
  }
  if (w > 0 && i > 0) {
    /* guess what the image is! */
    if (w < 32 && i < 48) {
      /* must be an icon or space */
      n = 1;
      if (strcasestr(p, "space") || strcasestr(p, "blank"))
        Strcat_charp(tmp, "_");
      else {
        if (w * i < 8 * 16)
          Strcat_charp(tmp, "*");
        else {
          if (!pre_int) {
            Strcat_charp(tmp, "<pre_int>");
            pre_int = true;
          }
          push_symbol(tmp, IMG_SYMBOL, symbol_width, 1);
          n = symbol_width;
        }
      }
      goto img_end;
    }
    if (w > 200 && i < 13) {
      /* must be a horizontal line */
      if (!pre_int) {
        Strcat_charp(tmp, "<pre_int>");
        pre_int = true;
      }
      w = w / pixel_per_char / symbol_width;
      if (w <= 0)
        w = 1;
      push_symbol(tmp, HR_SYMBOL, symbol_width, w);
      n = w * symbol_width;
      goto img_end;
    }
  }
  for (q = p; *q; q++)
    ;
  while (q > p && *q != '/')
    q--;
  if (*q == '/')
    q++;
  Strcat_char(tmp, '[');
  n = 1;
  p = q;
  for (; *q; q++) {
    if (!IS_ALNUM(*q) && *q != '_' && *q != '-') {
      break;
    }
    Strcat_char(tmp, *q);
    n++;
    if (n + 1 >= nw)
      break;
  }
  Strcat_char(tmp, ']');
  n++;
img_end:
  Strcat_charp(tmp, "</img_alt>");
  if (pre_int && !ext_pre_int)
    Strcat_charp(tmp, "</pre_int>");
  if (r) {
    Strcat_charp(tmp, "</input_alt>");
    parser->process_n_form();
  }
  return tmp;
}

Str *process_anchor(HtmlParser *parser, struct HtmlTag *tag,
                    const char *tagbuf) {
  if (tag->parsedtag_need_reconstruct()) {
    parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", parser->cur_hseq++)->ptr);
    return parsedtag2str(tag);
  } else {
    Str *tmp = Sprintf("<a hseq=\"%d\"", parser->cur_hseq++);
    Strcat_charp(tmp, tagbuf + 2);
    return tmp;
  }
}

Str *process_input(HtmlParser *parser, struct HtmlTag *tag) {
  int i = 20, v, x, y, z, iw, ih, size = 20;
  const char *q, *p, *r, *p2, *s;
  Str *tmp = NULL;
  auto qq = "";
  int qlen = 0;

  if (parser->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = parser->process_form(HtmlTag::parse(&s, true));
  }
  if (tmp == NULL)
    tmp = Strnew();

  p = "text";
  parsedtag_get_value(tag, ATTR_TYPE, &p);
  q = NULL;
  parsedtag_get_value(tag, ATTR_VALUE, &q);
  r = "";
  parsedtag_get_value(tag, ATTR_NAME, &r);
  parsedtag_get_value(tag, ATTR_SIZE, &size);
  if (size > MAX_INPUT_SIZE)
    size = MAX_INPUT_SIZE;
  parsedtag_get_value(tag, ATTR_MAXLENGTH, &i);
  p2 = NULL;
  parsedtag_get_value(tag, ATTR_ALT, &p2);
  x = tag->parsedtag_exists(ATTR_CHECKED);
  y = tag->parsedtag_exists(ATTR_ACCEPT);
  z = tag->parsedtag_exists(ATTR_READONLY);

  v = formtype(p);
  if (v == FORM_UNKNOWN)
    return NULL;

  if (!q) {
    switch (v) {
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      q = "SUBMIT";
      break;
    case FORM_INPUT_RESET:
      q = "RESET";
      break;
      /* if no VALUE attribute is specified in
       * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used
       * as a default value. It is not a part of HTML4.0
       * specification, but an imitation of Netscape behaviour.
       */
    case FORM_INPUT_CHECKBOX:
      q = "on";
    }
  }
  /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
  if (v == FORM_INPUT_FILE)
    q = NULL;
  if (q) {
    qq = html_quote(q);
    qlen = get_strwidth(q);
  }

  Strcat_charp(tmp, "<pre_int>");
  switch (v) {
  case FORM_INPUT_PASSWORD:
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_CHECKBOX:
    if (displayLinkNumber)
      Strcat(tmp, parser->getLinkNumberStr(0));
    Strcat_char(tmp, '[');
    break;
  case FORM_INPUT_RADIO:
    if (displayLinkNumber)
      Strcat(tmp, parser->getLinkNumberStr(0));
    Strcat_char(tmp, '(');
  }
  Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      parser->cur_hseq++, parser->cur_form_id(), html_quote(p),
                      html_quote(r), size, i, qq));
  if (x)
    Strcat_charp(tmp, " checked");
  if (y)
    Strcat_charp(tmp, " accept");
  if (z)
    Strcat_charp(tmp, " readonly");
  Strcat_char(tmp, '>');

  if (v == FORM_INPUT_HIDDEN)
    Strcat_charp(tmp, "</input_alt></pre_int>");
  else {
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      Strcat_charp(tmp, "<u>");
      break;
    case FORM_INPUT_IMAGE:
      s = NULL;
      parsedtag_get_value(tag, ATTR_SRC, &s);
      if (s) {
        Strcat(tmp, Sprintf("<img src=\"%s\"", html_quote(s)));
        if (p2)
          Strcat(tmp, Sprintf(" alt=\"%s\"", html_quote(p2)));
        if (parsedtag_get_value(tag, ATTR_WIDTH, &iw))
          Strcat(tmp, Sprintf(" width=\"%d\"", iw));
        if (parsedtag_get_value(tag, ATTR_HEIGHT, &ih))
          Strcat(tmp, Sprintf(" height=\"%d\"", ih));
        Strcat_charp(tmp, " pre_int>");
        Strcat_charp(tmp, "</input_alt></pre_int>");
        return tmp;
      }
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      if (displayLinkNumber)
        Strcat(tmp, parser->getLinkNumberStr(-1));
      Strcat_charp(tmp, "[");
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
      i = 0;
      if (q) {
        for (; i < qlen && i < size; i++)
          Strcat_char(tmp, '*');
      }
      for (; i < size; i++)
        Strcat_char(tmp, ' ');
      break;
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      if (q)
        Strcat(tmp, textfieldrep(Strnew_charp(q), size));
      else {
        for (i = 0; i < size; i++)
          Strcat_char(tmp, ' ');
      }
      break;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      if (p2)
        Strcat_charp(tmp, html_quote(p2));
      else
        Strcat_charp(tmp, qq);
      break;
    case FORM_INPUT_RESET:
      Strcat_charp(tmp, qq);
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (x)
        Strcat_char(tmp, '*');
      else
        Strcat_char(tmp, ' ');
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      Strcat_charp(tmp, "</u>");
      break;
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      Strcat_charp(tmp, "]");
    }
    Strcat_charp(tmp, "</input_alt>");
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
      Strcat_char(tmp, ']');
      break;
    case FORM_INPUT_RADIO:
      Strcat_char(tmp, ')');
    }
    Strcat_charp(tmp, "</pre_int>");
  }
  return tmp;
}

Str *process_button(HtmlParser *parser, struct HtmlTag *tag) {
  Str *tmp = NULL;
  const char *p, *q, *r, *qq = "";
  int v;

  if (parser->cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = parser->process_form(HtmlTag::parse(&s, true));
  }
  if (tmp == NULL)
    tmp = Strnew();

  p = "submit";
  parsedtag_get_value(tag, ATTR_TYPE, &p);
  q = NULL;
  parsedtag_get_value(tag, ATTR_VALUE, &q);
  r = "";
  parsedtag_get_value(tag, ATTR_NAME, &r);

  v = formtype(p);
  if (v == FORM_UNKNOWN)
    return NULL;

  switch (v) {
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
  case FORM_INPUT_RESET:
    break;
  default:
    p = "submit";
    v = FORM_INPUT_SUBMIT;
    break;
  }

  if (!q) {
    switch (v) {
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      q = "SUBMIT";
      break;
    case FORM_INPUT_RESET:
      q = "RESET";
      break;
    }
  }
  if (q) {
    qq = html_quote(q);
  }

  /*    Strcat_charp(tmp, "<pre_int>"); */
  Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                      "name=\"%s\" value=\"%s\">",
                      parser->cur_hseq++, parser->cur_form_id(), html_quote(p),
                      html_quote(r), qq));
  return tmp;
}

Str *process_n_button(void) {
  Str *tmp = Strnew();
  Strcat_charp(tmp, "</input_alt>");
  /*    Strcat_charp(tmp, "</pre_int>"); */
  return tmp;
}

Str *HtmlParser::process_select(struct HtmlTag *tag) {
  Str *tmp = NULL;

  if (cur_form_id() < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(HtmlTag::parse(&s, true));
  }

  auto p = "";
  parsedtag_get_value(tag, ATTR_NAME, &p);
  cur_select = Strnew_charp(p);
  select_is_multiple = tag->parsedtag_exists(ATTR_MULTIPLE);

  select_str = Strnew();
  cur_option = NULL;
  cur_status = R_ST_NORMAL;
  n_selectitem = 0;
  return tmp;
}

Str *HtmlParser::process_n_select() {
  if (cur_select == NULL)
    return NULL;
  this->process_option();
  Strcat_charp(select_str, "<br>");
  cur_select = NULL;
  n_selectitem = 0;
  return select_str;
}

void HtmlParser::feed_select(const char *str) {
  Str *tmp = Strnew();
  int prev_status = cur_status;
  static int prev_spaces = -1;
  const char *p;

  if (cur_select == NULL)
    return;
  while (read_token(tmp, &str, &cur_status, 0, 0)) {
    if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
      continue;
    p = tmp->ptr;
    if (tmp->ptr[0] == '<' && Strlastchar(tmp) == '>') {
      struct HtmlTag *tag;
      char *q;
      if (!(tag = HtmlTag::parse(&p, false)))
        continue;
      switch (tag->tagid) {
      case HTML_OPTION:
        this->process_option();
        cur_option = Strnew();
        if (parsedtag_get_value(tag, ATTR_VALUE, &q))
          cur_option_value = Strnew_charp(q);
        else
          cur_option_value = NULL;
        if (parsedtag_get_value(tag, ATTR_LABEL, &q))
          cur_option_label = Strnew_charp(q);
        else
          cur_option_label = NULL;
        cur_option_selected = tag->parsedtag_exists(ATTR_SELECTED);
        prev_spaces = -1;
        break;
      case HTML_N_OPTION:
        /* do nothing */
        break;
      default:
        /* never happen */
        break;
      }
    } else if (cur_option) {
      while (*p) {
        if (IS_SPACE(*p) && prev_spaces != 0) {
          p++;
          if (prev_spaces > 0)
            prev_spaces++;
        } else {
          if (IS_SPACE(*p))
            prev_spaces = 1;
          else
            prev_spaces = 0;
          if (*p == '&')
            Strcat_charp(cur_option, getescapecmd(&p));
          else
            Strcat_char(cur_option, *(p++));
        }
      }
    }
  }
}

void HtmlParser::process_option() {
  char begin_char = '[', end_char = ']';

  if (cur_select == NULL || cur_option == NULL)
    return;
  while (cur_option->length > 0 && IS_SPACE(Strlastchar(cur_option)))
    Strshrink(cur_option, 1);
  if (cur_option_value == NULL)
    cur_option_value = cur_option;
  if (cur_option_label == NULL)
    cur_option_label = cur_option;
  if (!select_is_multiple) {
    begin_char = '(';
    end_char = ')';
  }
  Strcat(select_str, Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, this->cur_hseq++, cur_form_id(),
                             select_is_multiple ? "checkbox" : "radio",
                             html_quote(cur_select->ptr),
                             html_quote(cur_option_value->ptr)));
  if (cur_option_selected)
    Strcat_charp(select_str, " checked>*</input_alt>");
  else
    Strcat_charp(select_str, "> </input_alt>");
  Strcat_char(select_str, end_char);
  Strcat_charp(select_str, html_quote(cur_option_label->ptr));
  Strcat_charp(select_str, "</pre_int>");
  n_selectitem++;
}

static Str *process_hr(struct HtmlTag *tag, int width, int indent_width) {
  Str *tmp = Strnew_charp("<nobr>");
  int w = 0;
  int x = ALIGN_CENTER;
#define HR_ATTR_WIDTH_MAX 65535

  if (width > indent_width)
    width -= indent_width;
  if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
    if (w > HR_ATTR_WIDTH_MAX) {
      w = HR_ATTR_WIDTH_MAX;
    }
    w = REAL_WIDTH(w, width);
  } else {
    w = width;
  }

  parsedtag_get_value(tag, ATTR_ALIGN, &x);
  switch (x) {
  case ALIGN_CENTER:
    Strcat_charp(tmp, "<div_int align=center>");
    break;
  case ALIGN_RIGHT:
    Strcat_charp(tmp, "<div_int align=right>");
    break;
  case ALIGN_LEFT:
    Strcat_charp(tmp, "<div_int align=left>");
    break;
  }
  w /= symbol_width;
  if (w <= 0)
    w = 1;
  push_symbol(tmp, HR_SYMBOL, symbol_width, w);
  Strcat_charp(tmp, "</div_int></nobr>");
  return tmp;
}

static void clear_ignore_p_flag(int cmd, struct readbuffer *obuf) {
  static int clear_flag_cmd[] = {HTML_HR, HTML_UNKNOWN};
  int i;

  for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++) {
    if (cmd == clear_flag_cmd[i]) {
      obuf->flag &= ~RB_IGNORE_P;
      return;
    }
  }
}

static void set_alignment(struct readbuffer *obuf, struct HtmlTag *tag) {
  long flag = -1;
  int align;

  if (parsedtag_get_value(tag, ATTR_ALIGN, &align)) {
    switch (align) {
    case ALIGN_CENTER:
      if (DisableCenter)
        flag = RB_LEFT;
      else
        flag = RB_CENTER;
      break;
    case ALIGN_RIGHT:
      flag = RB_RIGHT;
      break;
    case ALIGN_LEFT:
      flag = RB_LEFT;
    }
  }
  RB_SAVE_FLAG(obuf);
  if (flag != -1) {
    RB_SET_ALIGN(obuf, flag);
  }
}

#ifdef ID_EXT
static void process_idattr(HtmlParser *parser, struct readbuffer *obuf, int cmd,
                           struct HtmlTag *tag) {
  char *id = NULL, *framename = NULL;
  Str *idtag = NULL;

  /*
   * HTML_TABLE is handled by the other process.
   */
  if (cmd == HTML_TABLE)
    return;

  parsedtag_get_value(tag, ATTR_ID, &id);
  parsedtag_get_value(tag, ATTR_FRAMENAME, &framename);
  if (id == NULL)
    return;
  if (framename)
    idtag = Sprintf("<_id id=\"%s\" framename=\"%s\">", html_quote(id),
                    html_quote(framename));
  else
    idtag = Sprintf("<_id id=\"%s\">", html_quote(id));
  parser->push_tag(obuf, idtag->ptr, HTML_NOP);
}
#endif /* ID_EXT */

#define CLOSE_P                                                                \
  if (obuf->flag & RB_P) {                                                     \
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);         \
    RB_RESTORE_FLAG(obuf);                                                     \
    obuf->flag &= ~RB_P;                                                       \
  }

#define HTML5_CLOSE_A                                                          \
  do {                                                                         \
    if (obuf->flag & RB_HTML5) {                                               \
      this->close_anchor(h_env, obuf);                                         \
    }                                                                          \
  } while (0)

#define CLOSE_A                                                                \
  do {                                                                         \
    CLOSE_P;                                                                   \
    if (!(obuf->flag & RB_HTML5)) {                                            \
      parser->close_anchor(h_env, obuf);                                       \
    }                                                                          \
  } while (0)

#define CLOSE_DT                                                               \
  if (obuf->flag & RB_IN_DT) {                                                 \
    obuf->flag &= ~RB_IN_DT;                                                   \
    HTMLlineproc1("</b>", h_env);                                              \
  }

#define PUSH_ENV(cmd)                                                          \
  if (++h_env->envc_real < h_env->nenv) {                                      \
    ++h_env->envc;                                                             \
    envs[h_env->envc].env = cmd;                                               \
    envs[h_env->envc].count = 0;                                               \
    if (h_env->envc <= MAX_INDENT_LEVEL)                                       \
      envs[h_env->envc].indent = envs[h_env->envc - 1].indent + INDENT_INCR;   \
    else                                                                       \
      envs[h_env->envc].indent = envs[h_env->envc - 1].indent;                 \
  }

#define PUSH_ENV_NOINDENT(cmd)                                                 \
  if (++h_env->envc_real < h_env->nenv) {                                      \
    ++h_env->envc;                                                             \
    envs[h_env->envc].env = cmd;                                               \
    envs[h_env->envc].count = 0;                                               \
    envs[h_env->envc].indent = envs[h_env->envc - 1].indent;                   \
  }

#define POP_ENV                                                                \
  if (h_env->envc_real-- < h_env->nenv)                                        \
    h_env->envc--;

static int ul_type(struct HtmlTag *tag, int default_type) {
  char *p;
  if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
    if (!strcasecmp(p, "disc"))
      return (int)'d';
    else if (!strcasecmp(p, "circle"))
      return (int)'c';
    else if (!strcasecmp(p, "square"))
      return (int)'s';
  }
  return default_type;
}

int getMetaRefreshParam(const char *q, Str **refresh_uri) {
  int refresh_interval;
  const char *r;
  Str *s_tmp = NULL;

  if (q == NULL || refresh_uri == NULL)
    return 0;

  refresh_interval = atoi(q);
  if (refresh_interval < 0)
    return 0;

  while (*q) {
    if (!strncasecmp(q, "url=", 4)) {
      q += 4;
      if (*q == '\"' || *q == '\'') /* " or ' */
        q++;
      r = q;
      while (*r && !IS_SPACE(*r) && *r != ';')
        r++;
      s_tmp = Strnew_charp_n(q, r - q);

      if (s_tmp->length > 0 &&
          (s_tmp->ptr[s_tmp->length - 1] == '\"' ||  /* " */
           s_tmp->ptr[s_tmp->length - 1] == '\'')) { /* ' */
        s_tmp->length--;
        s_tmp->ptr[s_tmp->length] = '\0';
      }
      q = r;
    }
    while (*q && *q != ';')
      q++;
    if (*q == ';')
      q++;
    while (*q && *q == ' ')
      q++;
  }
  *refresh_uri = s_tmp;
  return refresh_interval;
}

int HtmlParser::HTMLtagproc1(struct HtmlTag *tag,
                             struct html_feed_environ *h_env) {
  auto parser = this;

  const char *p, *q, *r;
  int i, w, x, y, z, count, width;
  struct readbuffer *obuf = h_env->obuf;
  struct environment *envs = h_env->envs;
  Str *tmp;
  int hseq;
  HtmlCommand cmd;
#ifdef ID_EXT
  char *id = NULL;
#endif /* ID_EXT */

  cmd = tag->tagid;

  if (obuf->flag & RB_PRE) {
    switch (cmd) {
    case HTML_NOBR:
    case HTML_N_NOBR:
    case HTML_PRE_INT:
    case HTML_N_PRE_INT:
      return 1;

    default:
      break;
    }
  }

  switch (cmd) {
  case HTML_B:
    if (obuf->in_bold < FONTSTAT_MAX)
      obuf->in_bold++;
    if (obuf->in_bold > 1)
      return 1;
    return 0;
  case HTML_N_B:
    if (obuf->in_bold == 1 && close_effect0(obuf, HTML_B))
      obuf->in_bold = 0;
    if (obuf->in_bold > 0) {
      obuf->in_bold--;
      if (obuf->in_bold == 0)
        return 0;
    }
    return 1;
  case HTML_I:
    if (obuf->in_italic < FONTSTAT_MAX)
      obuf->in_italic++;
    if (obuf->in_italic > 1)
      return 1;
    return 0;
  case HTML_N_I:
    if (obuf->in_italic == 1 && close_effect0(obuf, HTML_I))
      obuf->in_italic = 0;
    if (obuf->in_italic > 0) {
      obuf->in_italic--;
      if (obuf->in_italic == 0)
        return 0;
    }
    return 1;
  case HTML_U:
    if (obuf->in_under < FONTSTAT_MAX)
      obuf->in_under++;
    if (obuf->in_under > 1)
      return 1;
    return 0;
  case HTML_N_U:
    if (obuf->in_under == 1 && close_effect0(obuf, HTML_U))
      obuf->in_under = 0;
    if (obuf->in_under > 0) {
      obuf->in_under--;
      if (obuf->in_under == 0)
        return 0;
    }
    return 1;
  case HTML_EM:
    HTMLlineproc1("<i>", h_env);
    return 1;
  case HTML_N_EM:
    HTMLlineproc1("</i>", h_env);
    return 1;
  case HTML_STRONG:
    HTMLlineproc1("<b>", h_env);
    return 1;
  case HTML_N_STRONG:
    HTMLlineproc1("</b>", h_env);
    return 1;
  case HTML_Q:
    HTMLlineproc1("`", h_env);
    return 1;
  case HTML_N_Q:
    HTMLlineproc1("'", h_env);
    return 1;
  case HTML_FIGURE:
  case HTML_N_FIGURE:
  case HTML_P:
  case HTML_N_P:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    obuf->flag |= RB_IGNORE_P;
    if (cmd == HTML_P) {
      set_alignment(obuf, tag);
      obuf->flag |= RB_P;
    }
    return 1;
  case HTML_FIGCAPTION:
  case HTML_N_FIGCAPTION:
  case HTML_BR:
    flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
    h_env->blank_lines = 0;
    return 1;
  case HTML_H:
    if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P))) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    HTMLlineproc1("<b>", h_env);
    set_alignment(obuf, tag);
    return 1;
  case HTML_N_H:
    HTMLlineproc1("</b>", h_env);
    if (!(obuf->flag & RB_PREMODE)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    this->close_anchor(h_env, obuf);
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_UL:
  case HTML_OL:
  case HTML_BLQ:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      if (!(obuf->flag & RB_PREMODE) && (h_env->envc == 0 || cmd == HTML_BLQ))
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    PUSH_ENV(cmd);
    if (cmd == HTML_UL || cmd == HTML_OL) {
      if (parsedtag_get_value(tag, ATTR_START, &count)) {
        envs[h_env->envc].count = count - 1;
      }
    }
    if (cmd == HTML_OL) {
      envs[h_env->envc].type = '1';
      if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
        envs[h_env->envc].type = (int)*p;
      }
    }
    if (cmd == HTML_UL)
      envs[h_env->envc].type = ul_type(tag, 0);
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    return 1;
  case HTML_N_UL:
  case HTML_N_OL:
  case HTML_N_DL:
  case HTML_N_BLQ:
  case HTML_N_DD:
    CLOSE_DT;
    CLOSE_A;
    if (h_env->envc > 0) {
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
      POP_ENV;
      if (!(obuf->flag & RB_PREMODE) &&
          (h_env->envc == 0 || cmd == HTML_N_BLQ)) {
        do_blankline(h_env, obuf, envs[h_env->envc].indent, INDENT_INCR,
                     h_env->limit);
        obuf->flag |= RB_IGNORE_P;
      }
    }
    this->close_anchor(h_env, obuf);
    return 1;
  case HTML_DL:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      if (!(obuf->flag & RB_PREMODE) && envs[h_env->envc].env != HTML_DL &&
          envs[h_env->envc].env != HTML_DL_COMPACT &&
          envs[h_env->envc].env != HTML_DD)
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    PUSH_ENV_NOINDENT(cmd);
    if (tag->parsedtag_exists(ATTR_COMPACT))
      envs[h_env->envc].env = HTML_DL_COMPACT;
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_LI:
    CLOSE_A;
    CLOSE_DT;
    if (h_env->envc > 0) {
      Str *num;
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
      envs[h_env->envc].count++;
      if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
        count = atoi(p);
        if (count > 0)
          envs[h_env->envc].count = count;
        else
          envs[h_env->envc].count = 0;
      }
      switch (envs[h_env->envc].env) {
      case HTML_UL:
        envs[h_env->envc].type = ul_type(tag, envs[h_env->envc].type);
        for (i = 0; i < INDENT_INCR - 3; i++)
          push_charp(obuf, 1, NBSP, PC_ASCII);
        tmp = Strnew();
        switch (envs[h_env->envc].type) {
        case 'd':
          push_symbol(tmp, UL_SYMBOL_DISC, symbol_width, 1);
          break;
        case 'c':
          push_symbol(tmp, UL_SYMBOL_CIRCLE, symbol_width, 1);
          break;
        case 's':
          push_symbol(tmp, UL_SYMBOL_SQUARE, symbol_width, 1);
          break;
        default:
          push_symbol(tmp, UL_SYMBOL((h_env->envc_real - 1) % MAX_UL_LEVEL),
                      symbol_width, 1);
          break;
        }
        if (symbol_width == 1)
          push_charp(obuf, 1, NBSP, PC_ASCII);
        push_str(obuf, symbol_width, tmp, PC_ASCII);
        push_charp(obuf, 1, NBSP, PC_ASCII);
        set_space_to_prevchar(obuf->prevchar);
        break;
      case HTML_OL:
        if (parsedtag_get_value(tag, ATTR_TYPE, &p))
          envs[h_env->envc].type = (int)*p;
        switch ((envs[h_env->envc].count > 0) ? envs[h_env->envc].type : '1') {
        case 'i':
          num = romanNumeral(envs[h_env->envc].count);
          break;
        case 'I':
          num = romanNumeral(envs[h_env->envc].count);
          Strupper(num);
          break;
        case 'a':
          num = romanAlphabet(envs[h_env->envc].count);
          break;
        case 'A':
          num = romanAlphabet(envs[h_env->envc].count);
          Strupper(num);
          break;
        default:
          num = Sprintf("%d", envs[h_env->envc].count);
          break;
        }
        if (INDENT_INCR >= 4)
          Strcat_charp(num, ". ");
        else
          Strcat_char(num, '.');
        push_spaces(obuf, 1, INDENT_INCR - num->length);
        push_str(obuf, num->length, num, PC_ASCII);
        if (INDENT_INCR >= 4)
          set_space_to_prevchar(obuf->prevchar);
        break;
      default:
        push_spaces(obuf, 1, INDENT_INCR);
        break;
      }
    } else {
      flushline(h_env, obuf, 0, 0, h_env->limit);
    }
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_DT:
    CLOSE_A;
    if (h_env->envc == 0 ||
        (h_env->envc_real < h_env->nenv && envs[h_env->envc].env != HTML_DL &&
         envs[h_env->envc].env != HTML_DL_COMPACT)) {
      PUSH_ENV_NOINDENT(HTML_DL);
    }
    if (h_env->envc > 0) {
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
    }
    if (!(obuf->flag & RB_IN_DT)) {
      HTMLlineproc1("<b>", h_env);
      obuf->flag |= RB_IN_DT;
    }
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_N_DT:
    if (!(obuf->flag & RB_IN_DT)) {
      return 1;
    }
    obuf->flag &= ~RB_IN_DT;
    HTMLlineproc1("</b>", h_env);
    if (h_env->envc > 0 && envs[h_env->envc].env == HTML_DL)
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
    return 1;
  case HTML_DD:
    CLOSE_A;
    CLOSE_DT;
    if (envs[h_env->envc].env == HTML_DL ||
        envs[h_env->envc].env == HTML_DL_COMPACT) {
      PUSH_ENV(HTML_DD);
    }

    if (h_env->envc > 0 && envs[h_env->envc - 1].env == HTML_DL_COMPACT) {
      if (obuf->pos > envs[h_env->envc].indent)
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      else
        push_spaces(obuf, 1, envs[h_env->envc].indent - obuf->pos);
    } else
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    /* obuf->flag |= RB_IGNORE_P; */
    return 1;
  case HTML_TITLE:
    this->close_anchor(h_env, obuf);
    process_title(tag);
    obuf->flag |= RB_TITLE;
    obuf->end_tag = HTML_N_TITLE;
    return 1;
  case HTML_N_TITLE:
    if (!(obuf->flag & RB_TITLE))
      return 1;
    obuf->flag &= ~RB_TITLE;
    obuf->end_tag = 0;
    tmp = process_n_title(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_TITLE_ALT:
    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
      h_env->title = html_unquote(p);
    return 0;
  case HTML_FRAMESET:
    PUSH_ENV(cmd);
    push_charp(obuf, 9, "--FRAME--", PC_ASCII);
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    return 0;
  case HTML_N_FRAMESET:
    if (h_env->envc > 0) {
      POP_ENV;
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    return 0;
  case HTML_NOFRAMES:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
    /* istr = str; */
    return 1;
  case HTML_N_NOFRAMES:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    obuf->flag &= ~RB_NOFRAMES;
    return 1;
  case HTML_FRAME:
    q = r = NULL;
    parsedtag_get_value(tag, ATTR_SRC, &q);
    parsedtag_get_value(tag, ATTR_NAME, &r);
    if (q) {
      q = html_quote(q);
      push_tag(obuf, Sprintf("<a hseq=\"%d\" href=\"%s\">", cur_hseq++, q)->ptr,
               HTML_A);
      if (r)
        q = html_quote(r);
      push_charp(obuf, get_strwidth(q), q, PC_ASCII);
      push_tag(obuf, "</a>", HTML_N_A);
    }
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    return 0;
  case HTML_HR:
    this->close_anchor(h_env, obuf);
    tmp = process_hr(tag, h_env->limit, envs[h_env->envc].indent);
    HTMLlineproc1(tmp->ptr, h_env);
    set_space_to_prevchar(obuf->prevchar);
    return 1;
  case HTML_PRE:
    x = tag->parsedtag_exists(ATTR_FOR_TABLE);
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      if (!x)
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    } else
      fillline(obuf, envs[h_env->envc].indent);
    obuf->flag |= (RB_PRE | RB_IGNORE_P);
    /* istr = str; */
    return 1;
  case HTML_N_PRE:
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    if (!(obuf->flag & RB_IGNORE_P)) {
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      obuf->flag |= RB_IGNORE_P;
      h_env->blank_lines++;
    }
    obuf->flag &= ~RB_PRE;
    this->close_anchor(h_env, obuf);
    return 1;
  case HTML_PRE_INT:
    i = obuf->line->length;
    append_tags(obuf);
    if (!(obuf->flag & RB_SPECIAL)) {
      set_breakpoint(obuf, obuf->line->length - i);
    }
    obuf->flag |= RB_PRE_INT;
    return 0;
  case HTML_N_PRE_INT:
    push_tag(obuf, "</pre_int>", HTML_N_PRE_INT);
    obuf->flag &= ~RB_PRE_INT;
    if (!(obuf->flag & RB_SPECIAL) && obuf->pos > obuf->bp.pos) {
      set_prevchar(obuf->prevchar, "", 0);
      obuf->prev_ctype = PC_CTRL;
    }
    return 1;
  case HTML_NOBR:
    obuf->flag |= RB_NOBR;
    obuf->nobr_level++;
    return 0;
  case HTML_N_NOBR:
    if (obuf->nobr_level > 0)
      obuf->nobr_level--;
    if (obuf->nobr_level == 0)
      obuf->flag &= ~RB_NOBR;
    return 0;
  case HTML_PRE_PLAIN:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    obuf->flag |= (RB_PRE | RB_IGNORE_P);
    return 1;
  case HTML_N_PRE_PLAIN:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      obuf->flag |= RB_IGNORE_P;
    }
    obuf->flag &= ~RB_PRE;
    return 1;
  case HTML_LISTING:
  case HTML_XMP:
  case HTML_PLAINTEXT:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    obuf->flag |= (RB_PLAIN | RB_IGNORE_P);
    switch (cmd) {
    case HTML_LISTING:
      obuf->end_tag = HTML_N_LISTING;
      break;
    case HTML_XMP:
      obuf->end_tag = HTML_N_XMP;
      break;
    case HTML_PLAINTEXT:
      obuf->end_tag = MAX_HTMLTAG;
      break;

    default:
      break;
    }
    return 1;
  case HTML_N_LISTING:
  case HTML_N_XMP:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      obuf->flag |= RB_IGNORE_P;
    }
    obuf->flag &= ~RB_PLAIN;
    obuf->end_tag = 0;
    return 1;
  case HTML_SCRIPT:
    obuf->flag |= RB_SCRIPT;
    obuf->end_tag = HTML_N_SCRIPT;
    return 1;
  case HTML_STYLE:
    obuf->flag |= RB_STYLE;
    obuf->end_tag = HTML_N_STYLE;
    return 1;
  case HTML_N_SCRIPT:
    obuf->flag &= ~RB_SCRIPT;
    obuf->end_tag = 0;
    return 1;
  case HTML_N_STYLE:
    obuf->flag &= ~RB_STYLE;
    obuf->end_tag = 0;
    return 1;
  case HTML_A:
    if (obuf->anchor.url) {
      this->close_anchor(h_env, obuf);
    }
    hseq = 0;

    if (parsedtag_get_value(tag, ATTR_HREF, &p))
      obuf->anchor.url = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
      obuf->anchor.target = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_REFERER, &p))
      obuf->anchor.option = {.referer = p};
    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
      obuf->anchor.title = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_ACCESSKEY, &p))
      obuf->anchor.accesskey = (unsigned char)*p;
    if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq))
      obuf->anchor.hseq = hseq;

    if (hseq == 0 && obuf->anchor.url) {
      obuf->anchor.hseq = cur_hseq;
      tmp = process_anchor(parser, tag, h_env->tagbuf->ptr);
      push_tag(obuf, tmp->ptr, HTML_A);
      return 1;
    }
    return 0;
  case HTML_N_A:
    this->close_anchor(h_env, obuf);
    return 1;
  case HTML_IMG:
    if (tag->parsedtag_exists(ATTR_USEMAP))
      HTML5_CLOSE_A;
    tmp = process_img(parser, tag, h_env->limit);
    if (need_number) {
      tmp = Strnew_m_charp(getLinkNumberStr(-1)->ptr, tmp->ptr, NULL);
      need_number = 0;
    }
    HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_IMG_ALT:
    if (parsedtag_get_value(tag, ATTR_SRC, &p))
      obuf->img_alt = Strnew_charp(p);
    return 0;
  case HTML_N_IMG_ALT:
    if (obuf->img_alt) {
      if (!close_effect0(obuf, HTML_IMG_ALT))
        push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
      obuf->img_alt = NULL;
    }
    return 1;
  case HTML_INPUT_ALT:
    i = 0;
    if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
      if ((short)i > obuf->top_margin)
        obuf->top_margin = (short)i;
    }
    i = 0;
    if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
      if ((short)i > obuf->bottom_margin)
        obuf->bottom_margin = (short)i;
    }
    if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq)) {
      obuf->input_alt.hseq = hseq;
    }
    if (parsedtag_get_value(tag, ATTR_FID, &i)) {
      obuf->input_alt.fid = i;
    }
    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
      obuf->input_alt.type = Strnew_charp(p);
    }
    if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
      obuf->input_alt.value = Strnew_charp(p);
    }
    if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
      obuf->input_alt.name = Strnew_charp(p);
    }
    obuf->input_alt.in = 1;
    return 0;
  case HTML_N_INPUT_ALT:
    if (obuf->input_alt.in) {
      if (!close_effect0(obuf, HTML_INPUT_ALT))
        push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
      obuf->input_alt.hseq = 0;
      obuf->input_alt.fid = -1;
      obuf->input_alt.in = 0;
      obuf->input_alt.type = NULL;
      obuf->input_alt.name = NULL;
      obuf->input_alt.value = NULL;
    }
    return 1;
  case HTML_TABLE:
    this->close_anchor(h_env, obuf);
    if (obuf->table_level + 1 >= MAX_TABLE)
      break;
    obuf->table_level++;
    w = BORDER_NONE;
    /* x: cellspacing, y: cellpadding */
    x = 2;
    y = 1;
    z = 0;
    width = 0;
    if (tag->parsedtag_exists(ATTR_BORDER)) {
      if (parsedtag_get_value(tag, ATTR_BORDER, &w)) {
        if (w > 2)
          w = BORDER_THICK;
        else if (w < 0) { /* weird */
          w = BORDER_THIN;
        }
      } else
        w = BORDER_THIN;
    }
    if (DisplayBorders && w == BORDER_NONE)
      w = BORDER_THIN;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &i)) {
      if (obuf->table_level == 0)
        width = REAL_WIDTH(i, h_env->limit - envs[h_env->envc].indent);
      else
        width = RELATIVE_WIDTH(i);
    }
    if (tag->parsedtag_exists(ATTR_HBORDER))
      w = BORDER_NOWIN;
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000
    parsedtag_get_value(tag, ATTR_CELLSPACING, &x);
    parsedtag_get_value(tag, ATTR_CELLPADDING, &y);
    parsedtag_get_value(tag, ATTR_VSPACE, &z);
    if (x < 0)
      x = 0;
    if (y < 0)
      y = 0;
    if (z < 0)
      z = 0;
    if (x > MAX_CELLSPACING)
      x = MAX_CELLSPACING;
    if (y > MAX_CELLPADDING)
      y = MAX_CELLPADDING;
    if (z > MAX_VSPACE)
      z = MAX_VSPACE;
#ifdef ID_EXT
    parsedtag_get_value(tag, ATTR_ID, &id);
#endif /* ID_EXT */
    tables[obuf->table_level] = begin_table(w, x, y, z);
#ifdef ID_EXT
    if (id != NULL)
      tables[obuf->table_level]->id = Strnew_charp(id);
#endif /* ID_EXT */
    table_mode[obuf->table_level].pre_mode = 0;
    table_mode[obuf->table_level].indent_level = 0;
    table_mode[obuf->table_level].nobr_level = 0;
    table_mode[obuf->table_level].caption = 0;
    table_mode[obuf->table_level].end_tag = 0; /* HTML_UNKNOWN */
#ifndef TABLE_EXPAND
    tables[obuf->table_level]->total_width = width;
#else
    tables[obuf->table_level]->real_width = width;
    tables[obuf->table_level]->total_width = 0;
#endif
    return 1;
  case HTML_N_TABLE:
    /* should be processed in HTMLlineproc() */
    return 1;
  case HTML_CENTER:
    CLOSE_A;
    if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_SAVE_FLAG(obuf);
    if (DisableCenter)
      RB_SET_ALIGN(obuf, RB_LEFT);
    else
      RB_SET_ALIGN(obuf, RB_CENTER);
    return 1;
  case HTML_N_CENTER:
    CLOSE_A;
    if (!(obuf->flag & RB_PREMODE))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    return 1;
  case HTML_DIV:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    set_alignment(obuf, tag);
    return 1;
  case HTML_N_DIV:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    return 1;
  case HTML_DIV_INT:
    CLOSE_P;
    if (!(obuf->flag & RB_IGNORE_P))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    set_alignment(obuf, tag);
    return 1;
  case HTML_N_DIV_INT:
    CLOSE_P;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    return 1;
  case HTML_FORM:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    tmp = process_form(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_N_FORM:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    obuf->flag |= RB_IGNORE_P;
    process_n_form();
    return 1;
  case HTML_INPUT:
    this->close_anchor(h_env, obuf);
    tmp = process_input(parser, tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_BUTTON:
    HTML5_CLOSE_A;
    tmp = process_button(parser, tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_N_BUTTON:
    tmp = process_n_button();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_SELECT:
    this->close_anchor(h_env, obuf);
    tmp = process_select(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    obuf->flag |= RB_INSELECT;
    obuf->end_tag = HTML_N_SELECT;
    return 1;
  case HTML_N_SELECT:
    obuf->flag &= ~RB_INSELECT;
    obuf->end_tag = 0;
    tmp = this->process_n_select();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_OPTION:
    /* nothing */
    return 1;
  case HTML_TEXTAREA:
    this->close_anchor(h_env, obuf);
    tmp = process_textarea(tag, h_env->limit);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    obuf->flag |= RB_INTXTA;
    obuf->end_tag = HTML_N_TEXTAREA;
    return 1;
  case HTML_N_TEXTAREA:
    obuf->flag &= ~RB_INTXTA;
    obuf->end_tag = 0;
    tmp = this->process_n_textarea();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_ISINDEX:
    p = "";
    q = "!CURRENT_URL!";
    parsedtag_get_value(tag, ATTR_PROMPT, &p);
    parsedtag_get_value(tag, ATTR_ACTION, &q);
    tmp = Strnew_m_charp("<form method=get action=\"", html_quote(q), "\">",
                         html_quote(p),
                         "<input type=text name=\"\" accept></form>", NULL);
    HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_DOCTYPE:
    if (!tag->parsedtag_exists(ATTR_PUBLIC)) {
      obuf->flag |= RB_HTML5;
    }
    return 1;
  case HTML_META:
    p = q = r = NULL;
    parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
    parsedtag_get_value(tag, ATTR_CONTENT, &q);
    if (p && q && !strcasecmp(p, "refresh")) {
      int refresh_interval;
      tmp = NULL;
      refresh_interval = getMetaRefreshParam(q, &tmp);
      if (tmp) {
        q = html_quote(tmp->ptr);
        tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
                      refresh_interval, q, q);
      } else if (refresh_interval > 0)
        tmp = Sprintf("Refresh (%d sec)", refresh_interval);
      if (tmp) {
        HTMLlineproc1(tmp->ptr, h_env);
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      }
    }
    return 1;
  case HTML_BASE:
  case HTML_MAP:
  case HTML_N_MAP:
  case HTML_AREA:
    return 0;
  case HTML_DEL:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag |= RB_DEL;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>[DEL:</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike < FONTSTAT_MAX)
        obuf->in_strike++;
      if (obuf->in_strike == 1) {
        push_tag(obuf, "<s>", HTML_S);
      }
      break;
    }
    return 1;
  case HTML_N_DEL:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag &= ~RB_DEL;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>:DEL]</U>", h_env);
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike == 0)
        return 1;
      if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
        obuf->in_strike = 0;
      if (obuf->in_strike > 0) {
        obuf->in_strike--;
        if (obuf->in_strike == 0) {
          push_tag(obuf, "</s>", HTML_N_S);
        }
      }
      break;
    }
    return 1;
  case HTML_S:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag |= RB_S;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>[S:</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike < FONTSTAT_MAX)
        obuf->in_strike++;
      if (obuf->in_strike == 1) {
        push_tag(obuf, "<s>", HTML_S);
      }
      break;
    }
    return 1;
  case HTML_N_S:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag &= ~RB_S;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>:S]</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike == 0)
        return 1;
      if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
        obuf->in_strike = 0;
      if (obuf->in_strike > 0) {
        obuf->in_strike--;
        if (obuf->in_strike == 0) {
          push_tag(obuf, "</s>", HTML_N_S);
        }
      }
    }
    return 1;
  case HTML_INS:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>[INS:</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_ins < FONTSTAT_MAX)
        obuf->in_ins++;
      if (obuf->in_ins == 1) {
        push_tag(obuf, "<ins>", HTML_INS);
      }
      break;
    }
    return 1;
  case HTML_N_INS:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>:INS]</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_ins == 0)
        return 1;
      if (obuf->in_ins == 1 && close_effect0(obuf, HTML_INS))
        obuf->in_ins = 0;
      if (obuf->in_ins > 0) {
        obuf->in_ins--;
        if (obuf->in_ins == 0) {
          push_tag(obuf, "</ins>", HTML_N_INS);
        }
      }
      break;
    }
    return 1;
  case HTML_SUP:
    if (!(obuf->flag & (RB_DEL | RB_S)))
      HTMLlineproc1("^", h_env);
    return 1;
  case HTML_N_SUP:
    return 1;
  case HTML_SUB:
    if (!(obuf->flag & (RB_DEL | RB_S)))
      HTMLlineproc1("[", h_env);
    return 1;
  case HTML_N_SUB:
    if (!(obuf->flag & (RB_DEL | RB_S)))
      HTMLlineproc1("]", h_env);
    return 1;
  case HTML_FONT:
  case HTML_N_FONT:
  case HTML_NOP:
    return 1;
  case HTML_BGSOUND:
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
        Str *s;
        q = html_quote(p);
        s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
    return 1;
  case HTML_EMBED:
    HTML5_CLOSE_A;
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
        Str *s;
        q = html_quote(p);
        s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
    return 1;
  case HTML_APPLET:
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_ARCHIVE, &p)) {
        Str *s;
        q = html_quote(p);
        s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
    return 1;
  case HTML_BODY:
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_BACKGROUND, &p)) {
        Str *s;
        q = html_quote(p);
        s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
  case HTML_N_HEAD:
    if (obuf->flag & RB_TITLE)
      HTMLlineproc1("</title>", h_env);
  case HTML_HEAD:
  case HTML_N_BODY:
    return 1;
  default:
    /* obuf->prevchar = '\0'; */
    return 0;
  }
  /* not reached */
  return 0;
}

void HtmlParser::proc_escape(struct readbuffer *obuf, const char **str_return) {
  const char *str = *str_return, *estr;
  int ech = getescapechar(str_return);
  int width, n_add = *str_return - str;
  Lineprop mode = PC_ASCII;

  if (ech < 0) {
    *str_return = str;
    proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
    return;
  }
  mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

  estr = conv_entity(ech);
  check_breakpoint(obuf, obuf->flag & RB_SPECIAL, estr);
  width = get_strwidth(estr);
  if (width == 1 && ech == (unsigned char)*estr && ech != '&' && ech != '<' &&
      ech != '>') {
    if (IS_CNTRL(ech))
      mode = PC_CTRL;
    push_charp(obuf, width, estr, mode);
  } else
    push_nchars(obuf, width, str, n_add, mode);
  set_prevchar(obuf->prevchar, estr, strlen(estr));
  obuf->prev_ctype = mode;
}

static int need_flushline(struct html_feed_environ *h_env,
                          struct readbuffer *obuf, Lineprop mode) {
  char ch;

  if (obuf->flag & RB_PRE_INT) {
    if (obuf->pos > h_env->limit)
      return 1;
    else
      return 0;
  }

  ch = Strlastchar(obuf->line);
  /* if (ch == ' ' && obuf->tag_sp > 0) */
  if (ch == ' ')
    return 0;

  if (obuf->pos > h_env->limit)
    return 1;

  return 0;
}

static int table_width(struct html_feed_environ *h_env, int table_level) {
  int width;
  if (table_level < 0)
    return 0;
  width = tables[table_level]->total_width;
  if (table_level > 0 || width > 0)
    return width;
  return h_env->limit - h_env->envs[h_env->envc].indent;
}

/* HTML processing first pass */
void HtmlParser::HTMLlineproc0(const char *line,
                               struct html_feed_environ *h_env, int internal) {
  Lineprop mode;
  int cmd;
  struct readbuffer *obuf = h_env->obuf;
  int indent, delta;
  struct HtmlTag *tag;
  Str *tokbuf;
  struct table *tbl = NULL;
  struct table_mode *tbl_mode = NULL;
  int tbl_width = 0;

#ifdef DEBUG
  if (w3m_debug) {
    FILE *f = fopen("zzzproc1", "a");
    fprintf(f, "%c%c%c%c", (obuf->flag & RB_PREMODE) ? 'P' : ' ',
            (obuf->table_level >= 0) ? 'T' : ' ',
            (obuf->flag & RB_INTXTA) ? 'X' : ' ',
            (obuf->flag & (RB_SCRIPT | RB_STYLE)) ? 'S' : ' ');
    fprintf(f, "HTMLlineproc1(\"%s\",%d,%lx)\n", line, h_env->limit,
            (unsigned long)h_env);
    fclose(f);
  }
#endif

  tokbuf = Strnew();

table_start:
  if (obuf->table_level >= 0) {
    int level = std::min<short>(obuf->table_level, MAX_TABLE - 1);
    tbl = tables[level];
    tbl_mode = &table_mode[level];
    tbl_width = table_width(h_env, level);
  }

  while (*line != '\0') {
    const char *str, *p;
    int is_tag = false;
    int pre_mode =
        (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->pre_mode : obuf->flag;
    int end_tag = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->end_tag
                                                       : obuf->end_tag;

    if (*line == '<' || obuf->status != R_ST_NORMAL) {
      /*
       * Tag processing
       */
      if (obuf->status == R_ST_EOL)
        obuf->status = R_ST_NORMAL;
      else {
        read_token(h_env->tagbuf, &line, &obuf->status, pre_mode & RB_PREMODE,
                   obuf->status != R_ST_NORMAL);
        if (obuf->status != R_ST_NORMAL)
          return;
      }
      if (h_env->tagbuf->length == 0)
        continue;
      str = h_env->tagbuf->Strdup()->ptr;
      if (*str == '<') {
        if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
          is_tag = true;
        else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT |
                               RB_STYLE | RB_TITLE))) {
          line = Strnew_m_charp(str + 1, line, NULL)->ptr;
          str = "&lt;";
        }
      }
    } else {
      read_token(tokbuf, &line, &obuf->status, pre_mode & RB_PREMODE, 0);
      if (obuf->status != R_ST_NORMAL) /* R_ST_AMP ? */
        obuf->status = R_ST_NORMAL;
      str = tokbuf->ptr;
      if (need_number) {
        str = Strnew_m_charp(getLinkNumberStr(-1)->ptr, str, NULL)->ptr;
        need_number = 0;
      }
    }

    if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE |
                    RB_TITLE)) {
      if (is_tag) {
        p = str;
        if ((tag = HtmlTag::parse(&p, internal))) {
          if (tag->tagid == end_tag ||
              (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM) ||
              (pre_mode & RB_TITLE &&
               (tag->tagid == HTML_N_HEAD || tag->tagid == HTML_BODY)))
            goto proc_normal;
        }
      }
      /* title */
      if (pre_mode & RB_TITLE) {
        feed_title(str);
        continue;
      }
      /* select */
      if (pre_mode & RB_INSELECT) {
        if (obuf->table_level >= 0)
          goto proc_normal;
        this->feed_select(str);
        continue;
      }
      if (is_tag) {
        if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<'))) {
          str = Strnew_charp_n(str, p - str)->ptr;
          line = Strnew_m_charp(p, line, NULL)->ptr;
        }
        is_tag = false;
        continue;
      }
      if (obuf->table_level >= 0)
        goto proc_normal;
      /* textarea */
      if (pre_mode & RB_INTXTA) {
        feed_textarea(str);
        continue;
      }
      /* script */
      if (pre_mode & RB_SCRIPT)
        continue;
      /* style */
      if (pre_mode & RB_STYLE)
        continue;
    }

  proc_normal:
    if (obuf->table_level >= 0 && tbl && tbl_mode) {
      /*
       * within table: in <table>..</table>, all input tokens
       * are fed to the table renderer, and then the renderer
       * makes HTML output.
       */
      switch (feed_table(this, tbl, str, tbl_mode, tbl_width, internal)) {
      case 0:
        /* </table> tag */
        obuf->table_level--;
        if (obuf->table_level >= MAX_TABLE - 1)
          continue;
        end_table(tbl);
        if (obuf->table_level >= 0) {
          struct table *tbl0 = tables[obuf->table_level];
          str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
          if (tbl0->row < 0)
            continue;
          pushTable(tbl0, tbl);
          tbl = tbl0;
          tbl_mode = &table_mode[obuf->table_level];
          tbl_width = table_width(h_env, obuf->table_level);
          feed_table(this, tbl, str, tbl_mode, tbl_width, true);
          continue;
          /* continue to the next */
        }
        if (obuf->flag & RB_DEL)
          continue;
        /* all tables have been read */
        if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P)) {
          int indent = h_env->envs[h_env->envc].indent;
          flushline(h_env, obuf, indent, 0, h_env->limit);
          do_blankline(h_env, obuf, indent, 0, h_env->limit);
        }
        save_fonteffect(h_env, obuf);
        initRenderTable();
        renderTable(this, tbl, tbl_width, h_env);
        restore_fonteffect(h_env, obuf);
        obuf->flag &= ~RB_IGNORE_P;
        if (tbl->vspace > 0) {
          int indent = h_env->envs[h_env->envc].indent;
          do_blankline(h_env, obuf, indent, 0, h_env->limit);
          obuf->flag |= RB_IGNORE_P;
        }
        set_space_to_prevchar(obuf->prevchar);
        continue;
      case 1:
        /* <table> tag */
        break;
      default:
        continue;
      }
    }

    if (is_tag) {
      /*** Beginning of a new tag ***/
      if ((tag = HtmlTag::parse(&str, internal)))
        cmd = tag->tagid;
      else
        continue;
      /* process tags */
      if (HTMLtagproc1(tag, h_env) == 0) {
        /* preserve the tag for second-stage processing */
        if (tag->parsedtag_need_reconstruct())
          h_env->tagbuf = parsedtag2str(tag);
        push_tag(obuf, h_env->tagbuf->ptr, cmd);
      }
#ifdef ID_EXT
      else {
        process_idattr(this, obuf, cmd, tag);
      }
#endif /* ID_EXT */
      obuf->bp.init_flag = 1;
      clear_ignore_p_flag(cmd, obuf);
      if (cmd == HTML_TABLE)
        goto table_start;
      else {
        if (displayLinkNumber && cmd == HTML_A && !internal)
          if (h_env->obuf->anchor.url)
            need_number = 1;
        continue;
      }
    }

    if (obuf->flag & (RB_DEL | RB_S))
      continue;
    while (*str) {
      mode = get_mctype(str);
      delta = get_mcwidth(str);
      if (obuf->flag & (RB_SPECIAL & ~RB_NOBR)) {
        char ch = *str;
        if (!(obuf->flag & RB_PLAIN) && (*str == '&')) {
          const char *p = str;
          int ech = getescapechar(&p);
          if (ech == '\n' || ech == '\r') {
            ch = '\n';
            str = p - 1;
          } else if (ech == '\t') {
            ch = '\t';
            str = p - 1;
          }
        }
        if (ch != '\n')
          obuf->flag &= ~RB_IGNORE_P;
        if (ch == '\n') {
          str++;
          if (obuf->flag & RB_IGNORE_P) {
            obuf->flag &= ~RB_IGNORE_P;
            continue;
          }
          if (obuf->flag & RB_PRE_INT)
            PUSH(' ');
          else
            flushline(h_env, obuf, h_env->envs[h_env->envc].indent, 1,
                      h_env->limit);
        } else if (ch == '\t') {
          do {
            PUSH(' ');
          } while ((h_env->envs[h_env->envc].indent + obuf->pos) % Tabstop !=
                   0);
          str++;
        } else if (obuf->flag & RB_PLAIN) {
          auto p = html_quote_char(*str);
          if (p) {
            push_charp(obuf, 1, p, PC_ASCII);
            str++;
          } else {
            proc_mchar(obuf, 1, delta, &str, mode);
          }
        } else {
          if (*str == '&')
            proc_escape(obuf, &str);
          else
            proc_mchar(obuf, 1, delta, &str, mode);
        }
        if (obuf->flag & (RB_SPECIAL & ~RB_PRE_INT))
          continue;
      } else {
        if (!IS_SPACE(*str))
          obuf->flag &= ~RB_IGNORE_P;
        if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str)) {
          if (*obuf->prevchar->ptr != ' ') {
            PUSH(' ');
          }
          str++;
        } else {
          if (*str == '&')
            proc_escape(obuf, &str);
          else
            proc_mchar(obuf, obuf->flag & RB_SPECIAL, delta, &str, mode);
        }
      }
      if (need_flushline(h_env, obuf, mode)) {
        char *bp = obuf->line->ptr + obuf->bp.len;
        char *tp = bp - obuf->bp.tlen;
        int i = 0;

        if (tp > obuf->line->ptr && tp[-1] == ' ')
          i = 1;

        indent = h_env->envs[h_env->envc].indent;
        if (obuf->bp.pos - i > indent) {
          Str *line;
          append_tags(obuf); /* may reallocate the buffer */
          bp = obuf->line->ptr + obuf->bp.len;
          line = Strnew_charp(bp);
          Strshrink(obuf->line, obuf->line->length - obuf->bp.len);
#ifdef FORMAT_NICE
          if (obuf->pos - i > h_env->limit)
            obuf->flag |= RB_FILL;
#endif /* FORMAT_NICE */
          back_to_breakpoint(obuf);
          flushline(h_env, obuf, indent, 0, h_env->limit);
#ifdef FORMAT_NICE
          obuf->flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
          HTMLlineproc1(line->ptr, h_env);
        }
      }
    }
  }
  if (!(obuf->flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
    char *tp;
    int i = 0;

    if (obuf->bp.pos == obuf->pos) {
      tp = &obuf->line->ptr[obuf->bp.len - obuf->bp.tlen];
    } else {
      tp = &obuf->line->ptr[obuf->line->length];
    }

    if (tp > obuf->line->ptr && tp[-1] == ' ')
      i = 1;
    indent = h_env->envs[h_env->envc].indent;
    if (obuf->pos - i > h_env->limit) {
#ifdef FORMAT_NICE
      obuf->flag |= RB_FILL;
#endif /* FORMAT_NICE */
      flushline(h_env, obuf, indent, 0, h_env->limit);
#ifdef FORMAT_NICE
      obuf->flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
    }
  }
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

static const char *_size_unit[] = {"b",  "kb", "Mb", "Gb", "Tb", "Pb",
                                   "Eb", "Zb", "Bb", "Yb", NULL};

char *convert_size(long long size, int usefloat) {
  float csize;
  int sizepos = 0;
  auto sizes = _size_unit;

  csize = (float)size;
  while (csize >= 999.495 && sizes[sizepos + 1]) {
    csize = csize / 1024.0;
    sizepos++;
  }
  return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
                 floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
      ->ptr;
}

char *convert_size2(long long size1, long long size2, int usefloat) {
  auto sizes = _size_unit;
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

static TextLineListItem *_tl_lp2;

static Str *textlist_feed(void) {
  TextLine *p;
  if (_tl_lp2 != NULL) {
    p = _tl_lp2->ptr;
    _tl_lp2 = _tl_lp2->next;
    return p->line;
  }
  return NULL;
}

static void HTMLlineproc2(HtmlParser *parser, HttpResponse *res,
                          LineLayout *layout, TextLineList *tl) {
  _tl_lp2 = tl->first;
  parser->HTMLlineproc2body(res, layout, textlist_feed, -1);
}

/*
 * loadHTMLBuffer: read file and make new buffer
 */
// void loadHTMLBuffer(UrlStream *f, const std::shared_ptr<HttpResponse> &res,
//                     LineLayout *layout) {
//
//   loadHTMLstream(f, res, layout, false /*newBuf->bufferprop*/);
// }

void loadHTMLstream(HttpResponse *res, LineLayout *layout, bool internal) {

  HtmlParser parser;

  auto src = res->createSourceFile();

  struct environment envs[MAX_ENV_LEVEL];
  long long linelen = 0;
  long long trbyte = 0;
  struct html_feed_environ htmlenv1;
  struct readbuffer obuf;
  MySignalHandler prevtrap = NULL;

  symbol_width = symbol_width0 = 1;

  init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, INIT_BUFFER_WIDTH(),
            0);

  htmlenv1.buf = newTextLineList();

  if (SETJMP(AbortLoading) != 0) {
    parser.HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1);
    goto phase2;
  }
  TRAP_ON;

  if (res->f.stream->IStype() != IST_ENCODED) {
    res->f.stream = newEncodedStream(res->f.stream, res->f.encoding);
  }

  while (true) {
    auto _lineBuf2 = res->f.stream->StrmyISgets();
    if (_lineBuf2.empty()) {
      break;
    }
    auto lineBuf2 = Strnew(_lineBuf2);

    if (src)
      Strfputs(lineBuf2, src);
    linelen += lineBuf2->length;
    // showProgress(&linelen, &trbyte);
    /*
     * if (frame_source)
     * continue;
     */
    cleanup_line(lineBuf2, HTML_MODE);
    parser.HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
  }
  if (obuf.status != R_ST_NORMAL) {
    parser.HTMLlineproc0("\n", &htmlenv1, internal);
  }
  obuf.status = R_ST_NORMAL;
  parser.completeHTMLstream(&htmlenv1, &obuf);
  parser.flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);

  if (htmlenv1.title) {
    layout->title = htmlenv1.title;
  }

phase2:
  res->trbyte = trbyte + linelen;
  TRAP_OFF;
  HTMLlineproc2(&parser, res, layout, htmlenv1.buf);

  layout->topLine = layout->firstLine;
  layout->lastLine = layout->currentLine;
  layout->currentLine = layout->firstLine;
  res->type = "text/html";
  // if (n_textarea)
  formResetBuffer(layout, layout->formitem().get());
  if (src) {
    fclose(src);
  }
}

void init_henv(struct html_feed_environ *h_env, struct readbuffer *obuf,
               struct environment *envs, int nenv, TextLineList *buf, int limit,
               int indent) {
  envs[0].indent = indent;

  obuf->line = Strnew();
  obuf->cprop = 0;
  obuf->pos = 0;
  obuf->prevchar = Strnew_size(8);
  set_space_to_prevchar(obuf->prevchar);
  obuf->flag = RB_IGNORE_P;
  obuf->flag_sp = 0;
  obuf->status = R_ST_NORMAL;
  obuf->table_level = -1;
  obuf->nobr_level = 0;
  obuf->q_level = 0;
  bzero((void *)&obuf->anchor, sizeof(obuf->anchor));
  obuf->img_alt = 0;
  obuf->input_alt.hseq = 0;
  obuf->input_alt.fid = -1;
  obuf->input_alt.in = 0;
  obuf->input_alt.type = NULL;
  obuf->input_alt.name = NULL;
  obuf->input_alt.value = NULL;
  obuf->in_bold = 0;
  obuf->in_italic = 0;
  obuf->in_under = 0;
  obuf->in_strike = 0;
  obuf->in_ins = 0;
  obuf->prev_ctype = PC_ASCII;
  obuf->tag_sp = 0;
  obuf->fontstat_sp = 0;
  obuf->top_margin = 0;
  obuf->bottom_margin = 0;
  obuf->bp.init_flag = 1;
  set_breakpoint(obuf, 0);

  h_env->buf = buf;
  h_env->f = NULL;
  h_env->obuf = obuf;
  h_env->tagbuf = Strnew();
  h_env->limit = limit;
  h_env->maxlimit = 0;
  h_env->envs = envs;
  h_env->nenv = nenv;
  h_env->envc = 0;
  h_env->envc_real = 0;
  h_env->title = NULL;
  h_env->blank_lines = 0;
}

void HtmlParser::completeHTMLstream(struct html_feed_environ *h_env,
                                    struct readbuffer *obuf) {
  this->close_anchor(h_env, obuf);
  if (obuf->img_alt) {
    push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
    obuf->img_alt = NULL;
  }
  if (obuf->input_alt.in) {
    push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
    obuf->input_alt.hseq = 0;
    obuf->input_alt.fid = -1;
    obuf->input_alt.in = 0;
    obuf->input_alt.type = NULL;
    obuf->input_alt.name = NULL;
    obuf->input_alt.value = NULL;
  }
  if (obuf->in_bold) {
    push_tag(obuf, "</b>", HTML_N_B);
    obuf->in_bold = 0;
  }
  if (obuf->in_italic) {
    push_tag(obuf, "</i>", HTML_N_I);
    obuf->in_italic = 0;
  }
  if (obuf->in_under) {
    push_tag(obuf, "</u>", HTML_N_U);
    obuf->in_under = 0;
  }
  if (obuf->in_strike) {
    push_tag(obuf, "</s>", HTML_N_S);
    obuf->in_strike = 0;
  }
  if (obuf->in_ins) {
    push_tag(obuf, "</ins>", HTML_N_INS);
    obuf->in_ins = 0;
  }
  if (obuf->flag & RB_INTXTA)
    this->HTMLlineproc1("</textarea>", h_env);
  /* for unbalanced select tag */
  if (obuf->flag & RB_INSELECT)
    this->HTMLlineproc1("</select>", h_env);
  if (obuf->flag & RB_TITLE)
    this->HTMLlineproc1("</title>", h_env);

  /* for unbalanced table tag */
  if (obuf->table_level >= MAX_TABLE)
    obuf->table_level = MAX_TABLE - 1;

  while (obuf->table_level >= 0) {
    int tmp = obuf->table_level;
    table_mode[obuf->table_level].pre_mode &=
        ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
    this->HTMLlineproc1("</table>", h_env);
    if (obuf->table_level >= tmp)
      break;
  }
}

void cleanup_line(Str *s, CleanupMode mode) {
  if (mode == RAW_MODE) {
    return;
  }

  if (s->length >= 2 && s->ptr[s->length - 2] == '\r' &&
      s->ptr[s->length - 1] == '\n') {
    Strshrink(s, 2);
    Strcat_char(s, '\n');
  } else if (Strlastchar(s) == '\r') {
    s->ptr[s->length - 1] = '\n';
  } else if (Strlastchar(s) != '\n') {
    Strcat_char(s, '\n');
  }

  if (mode != PAGER_MODE) {
    for (int i = 0; i < s->length; i++) {
      if (s->ptr[i] == '\0') {
        s->ptr[i] = ' ';
      }
    }
  }
}

const char *remove_space(const char *str) {
  const char *p, *q;

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

void loadBuffer(HttpResponse *res, LineLayout *layout) {
  auto src = res->createSourceFile();

  auto nlines = 0;
  if (res->f.stream->IStype() != IST_ENCODED) {
    res->f.stream = newEncodedStream(res->f.stream, res->f.encoding);
  }

  char pre_lbuf = '\0';
  while (true) {
    auto _lineBuf2 = res->f.stream->StrmyISgets(); //&& lineBuf2->length
    if (_lineBuf2.empty()) {
      break;
    }
    auto lineBuf2 = Strnew(_lineBuf2);
    if (src) {
      Strfputs(lineBuf2, src);
    }
    cleanup_line(lineBuf2, PAGER_MODE);
    if (squeezeBlankLine) {
      if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
        ++nlines;
        continue;
      }
      pre_lbuf = lineBuf2->ptr[0];
    }
    ++nlines;
    Strchop(lineBuf2);

    Lineprop *propBuffer = nullptr;
    lineBuf2 = checkType(lineBuf2, &propBuffer);
    layout->addnewline(lineBuf2->ptr, propBuffer, lineBuf2->length,
                       FOLD_BUFFER_WIDTH(), nlines);
  }
  layout->topLine = layout->firstLine;
  layout->lastLine = layout->currentLine;
  layout->currentLine = layout->firstLine;
  if (src) {
    fclose(src);
  }
  res->type = "text/plain";
}

std::shared_ptr<Buffer> loadHTMLString(Str *page) {
  auto newBuf = Buffer::create();
  newBuf->res->f = UrlStream(SCM_LOCAL, newStrStream(page->ptr));

  loadHTMLstream(newBuf->res.get(), &newBuf->layout, true);

  return newBuf;
}

#define SHELLBUFFERNAME "*Shellout*"
std::shared_ptr<Buffer> getshell(const char *cmd) {
  if (cmd == nullptr || *cmd == '\0') {
    return nullptr;
  }

  auto f = popen(cmd, "r");
  if (f == nullptr) {
    return nullptr;
  }

  auto buf = Buffer::create();
  buf->res->f = UrlStream(SCM_UNKNOWN, newFileStream(f, pclose));
  loadBuffer(buf->res.get(), &buf->layout);
  buf->res->filename = cmd;
  buf->layout.title = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  return buf;
}

void set_breakpoint(struct readbuffer *obuf, int tag_length) {
  obuf->bp.len = obuf->line->length;
  obuf->bp.pos = obuf->pos;
  obuf->bp.tlen = tag_length;
  obuf->bp.flag = obuf->flag;
#ifdef FORMAT_NICE
  obuf->bp.flag &= ~RB_FILL;
#endif /* FORMAT_NICE */
  obuf->bp.top_margin = obuf->top_margin;
  obuf->bp.bottom_margin = obuf->bottom_margin;

  if (!obuf->bp.init_flag)
    return;

  bcopy((void *)&obuf->anchor, (void *)&obuf->bp.anchor, sizeof(obuf->anchor));
  obuf->bp.img_alt = obuf->img_alt;
  obuf->bp.input_alt = obuf->input_alt;
  obuf->bp.in_bold = obuf->in_bold;
  obuf->bp.in_italic = obuf->in_italic;
  obuf->bp.in_under = obuf->in_under;
  obuf->bp.in_strike = obuf->in_strike;
  obuf->bp.in_ins = obuf->in_ins;
  obuf->bp.nobr_level = obuf->nobr_level;
  obuf->bp.prev_ctype = obuf->prev_ctype;
  obuf->bp.init_flag = 0;
}
