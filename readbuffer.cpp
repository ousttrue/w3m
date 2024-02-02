#include "readbuffer.h"
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
#include "indep.h"
#include "textlist.h"
#include "buffer.h"
#include "terms.h"
#include "istream.h"
#include "htmltag.h"
#include "proto.h"
#include "alloc.h"
#include "alarm.h"
#include "funcname1.h"
#include "tmpfile.h"
#include <math.h>
#include <algorithm>

#define url_quote_conv(x, c) url_quote(x)

bool pseudoInlines = true;
bool ignore_null_img_alt = true;
double pixel_per_char = DEFAULT_PIXEL_PER_CHAR;
int pixel_per_char_i = DEFAULT_PIXEL_PER_CHAR;
int displayLinkNumber = false;
bool DisableCenter = false;
bool showLineNum = false;
int Tabstop = 8;
int IndentIncr = 4;
bool DisplayBorders = false;
int displayInsDel = DISPLAY_INS_DEL_NORMAL;
bool view_unseenobject = true;
bool MetaRefresh = false;

#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */
#define REAL_WIDTH(w, limit)                                                   \
  (((w) >= 0) ? (int)((w) / pixel_per_char) : -(w) * (limit) / 100)

#define set_prevchar(x, y, n) Strcopy_charp_n((x), (y), (n))
#define set_space_to_prevchar(x) Strcopy_charp_n((x), " ", 1)

static int cur_hseq;
static int need_number = 0;

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

static Str *cur_title;
static Str *pre_title;
static Str *cur_select;
static Str *select_str;
static int select_is_multiple;
static int n_selectitem;
static Str *cur_option;
static Str *cur_option_value;
static Str *cur_option_label;
static int cur_option_selected;
static int cur_status;

static Str *cur_textarea;
Str **textarea_str;
static int cur_textarea_size;
static int cur_textarea_rows;
static int cur_textarea_readonly;
static int n_textarea;
static int ignore_nl_textarea;
int max_textarea = MAX_TEXTAREA;

struct link_stack {
  int cmd;
  short offset;
  short pos;
  struct link_stack *next;
};

static struct link_stack *link_stack = NULL;

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

#define INITIAL_FORM_SIZE 10
static FormList **forms;
static int *form_stack;
static int form_max = -1;
static int forms_size = 0;
#define cur_form_id ((form_sp >= 0) ? form_stack[form_sp] : -1)
static int form_sp = 0;

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

Str *getLinkNumberStr(int correction) {
  return Sprintf("[%d]", cur_hseq + correction);
}

static char *has_hidden_link(struct readbuffer *obuf, int cmd) {
  Str *line = obuf->line;
  struct link_stack *p;

  if (Strlastchar(line) != '>')
    return NULL;

  for (p = link_stack; p; p = p->next)
    if (p->cmd == cmd)
      break;
  if (!p)
    return NULL;

  if (obuf->pos == p->pos)
    return line->ptr + p->offset;

  return NULL;
}

static void push_link(int cmd, int offset, int pos) {
  auto p = (struct link_stack *)New(struct link_stack);
  p->cmd = cmd;
  p->offset = (short)offset;
  if (p->offset < 0)
    p->offset = 0;
  p->pos = (short)pos;
  if (p->pos < 0)
    p->pos = 0;
  p->next = link_stack;
  link_stack = p;
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

static void set_breakpoint(struct readbuffer *obuf, int tag_length) {
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

static void append_tags(struct readbuffer *obuf) {
  int i;
  int len = obuf->line->length;
  int set_bp = 0;

  for (i = 0; i < obuf->tag_sp; i++) {
    switch (obuf->tag_stack[i]->cmd) {
    case HTML_A:
    case HTML_IMG_ALT:
    case HTML_B:
    case HTML_U:
    case HTML_I:
    case HTML_S:
      push_link(obuf->tag_stack[i]->cmd, obuf->line->length, obuf->pos);
      break;
    }
    Strcat_charp(obuf->line, obuf->tag_stack[i]->cmdname);
    switch (obuf->tag_stack[i]->cmd) {
    case HTML_NOBR:
      if (obuf->nobr_level > 1)
        break;
    case HTML_WBR:
      set_bp = 1;
      break;
    }
  }
  obuf->tag_sp = 0;
  if (set_bp)
    set_breakpoint(obuf, obuf->line->length - len);
}

static void push_tag(struct readbuffer *obuf, const char *cmdname, int cmd) {
  obuf->tag_stack[obuf->tag_sp] = (struct cmdtable *)New(struct cmdtable);
  obuf->tag_stack[obuf->tag_sp]->cmdname = allocStr(cmdname, -1);
  obuf->tag_stack[obuf->tag_sp]->cmd = cmd;
  obuf->tag_sp++;
  if (obuf->tag_sp >= TAG_STACK_SIZE || obuf->flag & (RB_SPECIAL & ~RB_NOBR))
    append_tags(obuf);
}

static void push_nchars(struct readbuffer *obuf, int width, const char *str,
                        int len, Lineprop mode) {
  append_tags(obuf);
  Strcat_charp_n(obuf->line, str, len);
  obuf->pos += width;
  if (width > 0) {
    set_prevchar(obuf->prevchar, str, len);
    obuf->prev_ctype = mode;
  }
  obuf->flag |= RB_NFLUSHED;
}

#define push_charp(obuf, width, str, mode)                                     \
  push_nchars(obuf, width, str, strlen(str), mode)

#define push_str(obuf, width, str, mode)                                       \
  push_nchars(obuf, width, str->ptr, str->length, mode)

static void check_breakpoint(struct readbuffer *obuf, int pre_mode,
                             const char *ch) {
  int tlen, len = obuf->line->length;

  append_tags(obuf);
  if (pre_mode)
    return;
  tlen = obuf->line->length - len;
  if (tlen > 0 ||
      is_boundary((unsigned char *)obuf->prevchar->ptr, (unsigned char *)ch))
    set_breakpoint(obuf, tlen);
}

static void push_char(struct readbuffer *obuf, int pre_mode, char ch) {
  check_breakpoint(obuf, pre_mode, &ch);
  Strcat_char(obuf->line, ch);
  obuf->pos++;
  set_prevchar(obuf->prevchar, &ch, 1);
  if (ch != ' ')
    obuf->prev_ctype = PC_ASCII;
  obuf->flag |= RB_NFLUSHED;
}

#define PUSH(c) push_char(obuf, obuf->flag &RB_SPECIAL, c)

static void push_spaces(struct readbuffer *obuf, int pre_mode, int width) {
  int i;

  if (width <= 0)
    return;
  check_breakpoint(obuf, pre_mode, " ");
  for (i = 0; i < width; i++)
    Strcat_char(obuf->line, ' ');
  obuf->pos += width;
  set_space_to_prevchar(obuf->prevchar);
  obuf->flag |= RB_NFLUSHED;
}

static void proc_mchar(struct readbuffer *obuf, int pre_mode, int width,
                       const char **str, Lineprop mode) {
  check_breakpoint(obuf, pre_mode, *str);
  obuf->pos += width;
  Strcat_charp_n(obuf->line, *str, get_mclen(*str));
  if (width > 0) {
    set_prevchar(obuf->prevchar, *str, 1);
    if (**str != ' ')
      obuf->prev_ctype = mode;
  }
  (*str) += get_mclen(*str);
  obuf->flag |= RB_NFLUSHED;
}

void push_render_image(Str *str, int width, int limit,
                       struct html_feed_environ *h_env) {
  struct readbuffer *obuf = h_env->obuf;
  int indent = h_env->envs[h_env->envc].indent;

  push_spaces(obuf, 1, (limit - width) / 2);
  push_str(obuf, width, str, PC_ASCII);
  push_spaces(obuf, 1, (limit - width + 1) / 2);
  if (width > 0)
    flushline(h_env, obuf, indent, 0, h_env->limit);
}

static int sloppy_parse_line(char **str) {
  if (**str == '<') {
    while (**str && **str != '>')
      (*str)++;
    if (**str == '>')
      (*str)++;
    return 1;
  } else {
    while (**str && **str != '<')
      (*str)++;
    return 0;
  }
}

#define MAX_CMD_LEN 128

int gethtmlcmd(const char **s) {
  extern Hash_si tagtable;
  char cmdstr[MAX_CMD_LEN];
  const char *p = cmdstr;
  const char *save = *s;
  int cmd;

  (*s)++;
  /* first character */
  if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  } else
    return HTML_UNKNOWN;
  if (p[-1] == '/')
    SKIP_BLANKS(*s);
  while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN) {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  }
  if (p - cmdstr == MAX_CMD_LEN) {
    /* buffer overflow: perhaps caused by bad HTML source */
    *s = save + 1;
    return HTML_UNKNOWN;
  }
  *(char *)p = '\0';

  /* hash search */
  cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
  while (**s && **s != '>')
    (*s)++;
  if (**s == '>')
    (*s)++;
  return cmd;
}

static void passthrough(struct readbuffer *obuf, char *str, int back) {
  int cmd;
  Str *tok = Strnew();
  char *str_bak;

  if (back) {
    Str *str_save = Strnew_charp(str);
    Strshrink(obuf->line, obuf->line->ptr + obuf->line->length - str);
    str = str_save->ptr;
  }
  while (*str) {
    str_bak = str;
    if (sloppy_parse_line(&str)) {
      const char *q = str_bak;
      cmd = gethtmlcmd(&q);
      if (back) {
        struct link_stack *p;
        for (p = link_stack; p; p = p->next) {
          if (p->cmd == cmd) {
            link_stack = p->next;
            break;
          }
        }
        back = 0;
      } else {
        Strcat_charp_n(tok, str_bak, str - str_bak);
        push_tag(obuf, tok->ptr, cmd);
        Strclear(tok);
      }
    } else {
      push_nchars(obuf, 0, str_bak, str - str_bak, obuf->prev_ctype);
    }
  }
}

static void fillline(struct readbuffer *obuf, int indent) {
  push_spaces(obuf, 1, indent - obuf->pos);
  obuf->flag &= ~RB_NFLUSHED;
}

void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
               int indent, int force, int width) {
  TextLineList *buf = h_env->buf;
  FILE *f = h_env->f;
  Str *line = obuf->line, *pass = NULL;
  char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
       *hidden_under = NULL, *hidden_italic = NULL, *hidden_strike = NULL,
       *hidden_ins = NULL, *hidden_input = NULL, *hidden = NULL;

#ifdef DEBUG
  if (w3m_debug) {
    FILE *df = fopen("zzzproc1", "a");
    fprintf(df, "flushline(%s,%d,%d,%d)\n", obuf->line->ptr, indent, force,
            width);
    if (buf) {
      TextLineListItem *p;
      for (p = buf->first; p; p = p->next) {
        fprintf(df, "buf=\"%s\"\n", p->ptr->line->ptr);
      }
    }
    fclose(df);
  }
#endif

  if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && Strlastchar(line) == ' ') {
    Strshrink(line, 1);
    obuf->pos--;
  }

  append_tags(obuf);

  if (obuf->anchor.url)
    hidden = hidden_anchor = has_hidden_link(obuf, HTML_A);
  if (obuf->img_alt) {
    if ((hidden_img = has_hidden_link(obuf, HTML_IMG_ALT)) != NULL) {
      if (!hidden || hidden_img < hidden)
        hidden = hidden_img;
    }
  }
  if (obuf->input_alt.in) {
    if ((hidden_input = has_hidden_link(obuf, HTML_INPUT_ALT)) != NULL) {
      if (!hidden || hidden_input < hidden)
        hidden = hidden_input;
    }
  }
  if (obuf->in_bold) {
    if ((hidden_bold = has_hidden_link(obuf, HTML_B)) != NULL) {
      if (!hidden || hidden_bold < hidden)
        hidden = hidden_bold;
    }
  }
  if (obuf->in_italic) {
    if ((hidden_italic = has_hidden_link(obuf, HTML_I)) != NULL) {
      if (!hidden || hidden_italic < hidden)
        hidden = hidden_italic;
    }
  }
  if (obuf->in_under) {
    if ((hidden_under = has_hidden_link(obuf, HTML_U)) != NULL) {
      if (!hidden || hidden_under < hidden)
        hidden = hidden_under;
    }
  }
  if (obuf->in_strike) {
    if ((hidden_strike = has_hidden_link(obuf, HTML_S)) != NULL) {
      if (!hidden || hidden_strike < hidden)
        hidden = hidden_strike;
    }
  }
  if (obuf->in_ins) {
    if ((hidden_ins = has_hidden_link(obuf, HTML_INS)) != NULL) {
      if (!hidden || hidden_ins < hidden)
        hidden = hidden_ins;
    }
  }
  if (hidden) {
    pass = Strnew_charp(hidden);
    Strshrink(line, line->ptr + line->length - hidden);
  }

  if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && obuf->pos > width) {
    char *tp = &line->ptr[obuf->bp.len - obuf->bp.tlen];
    char *ep = &line->ptr[line->length];

    if (obuf->bp.pos == obuf->pos && tp <= ep && tp > line->ptr &&
        tp[-1] == ' ') {
      bcopy(tp, tp - 1, ep - tp + 1);
      line->length--;
      obuf->pos--;
    }
  }

  if (obuf->anchor.url && !hidden_anchor)
    Strcat_charp(line, "</a>");
  if (obuf->img_alt && !hidden_img)
    Strcat_charp(line, "</img_alt>");
  if (obuf->input_alt.in && !hidden_input)
    Strcat_charp(line, "</input_alt>");
  if (obuf->in_bold && !hidden_bold)
    Strcat_charp(line, "</b>");
  if (obuf->in_italic && !hidden_italic)
    Strcat_charp(line, "</i>");
  if (obuf->in_under && !hidden_under)
    Strcat_charp(line, "</u>");
  if (obuf->in_strike && !hidden_strike)
    Strcat_charp(line, "</s>");
  if (obuf->in_ins && !hidden_ins)
    Strcat_charp(line, "</ins>");

  if (obuf->top_margin > 0) {
    int i;
    struct html_feed_environ h;
    struct readbuffer o;
    struct environment e[1];

    init_henv(&h, &o, e, 1, NULL, width, indent);
    o.line = Strnew_size(width + 20);
    o.pos = obuf->pos;
    o.flag = obuf->flag;
    o.top_margin = -1;
    o.bottom_margin = -1;
    Strcat_charp(o.line, "<pre_int>");
    for (i = 0; i < o.pos; i++)
      Strcat_char(o.line, ' ');
    Strcat_charp(o.line, "</pre_int>");
    for (i = 0; i < obuf->top_margin; i++)
      flushline(h_env, &o, indent, force, width);
  }

  if (force == 1 || obuf->flag & RB_NFLUSHED) {
    TextLine *lbuf = newTextLine(line, obuf->pos);
    if (RB_GET_ALIGN(obuf) == RB_CENTER) {
      align(lbuf, width, ALIGN_CENTER);
    } else if (RB_GET_ALIGN(obuf) == RB_RIGHT) {
      align(lbuf, width, ALIGN_RIGHT);
    } else if (RB_GET_ALIGN(obuf) == RB_LEFT && obuf->flag & RB_INTABLE) {
      align(lbuf, width, ALIGN_LEFT);
    }
#ifdef FORMAT_NICE
    else if (obuf->flag & RB_FILL) {
      char *p;
      int rest, rrest;
      int nspace, d, i;

      rest = width - get_Str_strwidth(line);
      if (rest > 1) {
        nspace = 0;
        for (p = line->ptr + indent; *p; p++) {
          if (*p == ' ')
            nspace++;
        }
        if (nspace > 0) {
          int indent_here = 0;
          d = rest / nspace;
          p = line->ptr;
          while (IS_SPACE(*p)) {
            p++;
            indent_here++;
          }
          rrest = rest - d * nspace;
          line = Strnew_size(width + 1);
          for (i = 0; i < indent_here; i++)
            Strcat_char(line, ' ');
          for (; *p; p++) {
            Strcat_char(line, *p);
            if (*p == ' ') {
              for (i = 0; i < d; i++)
                Strcat_char(line, ' ');
              if (rrest > 0) {
                Strcat_char(line, ' ');
                rrest--;
              }
            }
          }
          lbuf = newTextLine(line, width);
        }
      }
    }
#endif /* FORMAT_NICE */
#ifdef TABLE_DEBUG
    if (w3m_debug) {
      FILE *f = fopen("zzzproc1", "a");
      fprintf(f, "pos=%d,%d, maxlimit=%d\n", visible_length(lbuf->line->ptr),
              lbuf->pos, h_env->maxlimit);
      fclose(f);
    }
#endif
    if (lbuf->pos > h_env->maxlimit)
      h_env->maxlimit = lbuf->pos;
    if (buf)
      pushTextLine(buf, lbuf);
    else if (f) {
      Strfputs(lbuf->line, f);
      fputc('\n', f);
    }
    if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
      h_env->blank_lines = 0;
    else
      h_env->blank_lines++;
  } else {
    char *p = line->ptr, *q;
    Str *tmp = Strnew(), *tmp2 = Strnew();

#define APPEND(str)                                                            \
  if (buf)                                                                     \
    appendTextLine(buf, (str), 0);                                             \
  else if (f)                                                                  \
  Strfputs((str), f)

    while (*p) {
      q = p;
      if (sloppy_parse_line(&p)) {
        Strcat_charp_n(tmp, q, p - q);
        if (force == 2) {
          APPEND(tmp);
        } else
          Strcat(tmp2, tmp);
        Strclear(tmp);
      }
    }
    if (force == 2) {
      if (pass) {
        APPEND(pass);
      }
      pass = NULL;
    } else {
      if (pass)
        Strcat(tmp2, pass);
      pass = tmp2;
    }
  }

  if (obuf->bottom_margin > 0) {
    int i;
    struct html_feed_environ h;
    struct readbuffer o;
    struct environment e[1];

    init_henv(&h, &o, e, 1, NULL, width, indent);
    o.line = Strnew_size(width + 20);
    o.pos = obuf->pos;
    o.flag = obuf->flag;
    o.top_margin = -1;
    o.bottom_margin = -1;
    Strcat_charp(o.line, "<pre_int>");
    for (i = 0; i < o.pos; i++)
      Strcat_char(o.line, ' ');
    Strcat_charp(o.line, "</pre_int>");
    for (i = 0; i < obuf->bottom_margin; i++)
      flushline(h_env, &o, indent, force, width);
  }
  if (obuf->top_margin < 0 || obuf->bottom_margin < 0)
    return;

  obuf->line = Strnew_size(256);
  obuf->pos = 0;
  obuf->top_margin = 0;
  obuf->bottom_margin = 0;
  set_space_to_prevchar(obuf->prevchar);
  obuf->bp.init_flag = 1;
  obuf->flag &= ~RB_NFLUSHED;
  set_breakpoint(obuf, 0);
  obuf->prev_ctype = PC_ASCII;
  link_stack = NULL;
  fillline(obuf, indent);
  if (pass)
    passthrough(obuf, pass->ptr, 0);
  if (!hidden_anchor && obuf->anchor.url) {
    Str *tmp;
    if (obuf->anchor.hseq > 0)
      obuf->anchor.hseq = -obuf->anchor.hseq;
    tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor.hseq);
    Strcat_charp(tmp, html_quote(obuf->anchor.url));
    if (obuf->anchor.target) {
      Strcat_charp(tmp, "\" TARGET=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.target));
    }
    if (obuf->anchor.referer) {
      Strcat_charp(tmp, "\" REFERER=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.referer));
    }
    if (obuf->anchor.title) {
      Strcat_charp(tmp, "\" TITLE=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.title));
    }
    if (obuf->anchor.accesskey) {
      auto c = html_quote_char(obuf->anchor.accesskey);
      Strcat_charp(tmp, "\" ACCESSKEY=\"");
      if (c)
        Strcat_charp(tmp, c);
      else
        Strcat_char(tmp, obuf->anchor.accesskey);
    }
    Strcat_charp(tmp, "\">");
    push_tag(obuf, tmp->ptr, HTML_A);
  }
  if (!hidden_img && obuf->img_alt) {
    Str *tmp = Strnew_charp("<IMG_ALT SRC=\"");
    Strcat_charp(tmp, html_quote(obuf->img_alt->ptr));
    Strcat_charp(tmp, "\">");
    push_tag(obuf, tmp->ptr, HTML_IMG_ALT);
  }
  if (!hidden_input && obuf->input_alt.in) {
    Str *tmp;
    if (obuf->input_alt.hseq > 0)
      obuf->input_alt.hseq = -obuf->input_alt.hseq;
    tmp = Sprintf("<INPUT_ALT hseq=\"%d\" fid=\"%d\" name=\"%s\" type=\"%s\" "
                  "value=\"%s\">",
                  obuf->input_alt.hseq, obuf->input_alt.fid,
                  obuf->input_alt.name ? obuf->input_alt.name->ptr : "",
                  obuf->input_alt.type ? obuf->input_alt.type->ptr : "",
                  obuf->input_alt.value ? obuf->input_alt.value->ptr : "");
    push_tag(obuf, tmp->ptr, HTML_INPUT_ALT);
  }
  if (!hidden_bold && obuf->in_bold)
    push_tag(obuf, "<B>", HTML_B);
  if (!hidden_italic && obuf->in_italic)
    push_tag(obuf, "<I>", HTML_I);
  if (!hidden_under && obuf->in_under)
    push_tag(obuf, "<U>", HTML_U);
  if (!hidden_strike && obuf->in_strike)
    push_tag(obuf, "<S>", HTML_S);
  if (!hidden_ins && obuf->in_ins)
    push_tag(obuf, "<INS>", HTML_INS);
}

void do_blankline(struct html_feed_environ *h_env, struct readbuffer *obuf,
                  int indent, int indent_incr, int width) {
  if (h_env->blank_lines == 0)
    flushline(h_env, obuf, indent, 1, width);
}

void purgeline(struct html_feed_environ *h_env) {
  char *p, *q;
  Str *tmp;
  TextLine *tl;

  if (h_env->buf == NULL || h_env->blank_lines == 0)
    return;

  if (!(tl = rpopTextLine(h_env->buf)))
    return;
  p = tl->line->ptr;
  tmp = Strnew();
  while (*p) {
    q = p;
    if (sloppy_parse_line(&p)) {
      Strcat_charp_n(tmp, q, p - q);
    }
  }
  appendTextLine(h_env->buf, tmp, 0);
  h_env->blank_lines--;
}

static int close_effect0(struct readbuffer *obuf, int cmd) {
  int i;
  char *p;

  for (i = obuf->tag_sp - 1; i >= 0; i--) {
    if (obuf->tag_stack[i]->cmd == cmd)
      break;
  }
  if (i >= 0) {
    obuf->tag_sp--;
    bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
          (obuf->tag_sp - i) * sizeof(struct cmdtable *));
    return 1;
  } else if ((p = has_hidden_link(obuf, cmd)) != NULL) {
    passthrough(obuf, p, 1);
    return 1;
  }
  return 0;
}

static void close_anchor(struct html_feed_environ *h_env,
                         struct readbuffer *obuf) {
  if (obuf->anchor.url) {
    int i;
    char *p = NULL;
    int is_erased = 0;

    for (i = obuf->tag_sp - 1; i >= 0; i--) {
      if (obuf->tag_stack[i]->cmd == HTML_A)
        break;
    }
    if (i < 0 && obuf->anchor.hseq > 0 && Strlastchar(obuf->line) == ' ') {
      Strshrink(obuf->line, 1);
      obuf->pos--;
      is_erased = 1;
    }

    if (i >= 0 || (p = has_hidden_link(obuf, HTML_A))) {
      if (obuf->anchor.hseq > 0) {
        HTMLlineproc1(ANSP, h_env);
        set_space_to_prevchar(obuf->prevchar);
      } else {
        if (i >= 0) {
          obuf->tag_sp--;
          bcopy(&obuf->tag_stack[i + 1], &obuf->tag_stack[i],
                (obuf->tag_sp - i) * sizeof(struct cmdtable *));
        } else {
          passthrough(obuf, p, 1);
        }
        bzero((void *)&obuf->anchor, sizeof(obuf->anchor));
        return;
      }
      is_erased = 0;
    }
    if (is_erased) {
      Strcat_char(obuf->line, ' ');
      obuf->pos++;
    }

    push_tag(obuf, "</a>", HTML_N_A);
  }
  bzero((void *)&obuf->anchor, sizeof(obuf->anchor));
}

void save_fonteffect(struct html_feed_environ *h_env, struct readbuffer *obuf) {
  if (obuf->fontstat_sp < FONT_STACK_SIZE)
    bcopy(obuf->fontstat, obuf->fontstat_stack[obuf->fontstat_sp],
          FONTSTAT_SIZE);
  if (obuf->fontstat_sp < INT_MAX)
    obuf->fontstat_sp++;
  if (obuf->in_bold)
    push_tag(obuf, "</b>", HTML_N_B);
  if (obuf->in_italic)
    push_tag(obuf, "</i>", HTML_N_I);
  if (obuf->in_under)
    push_tag(obuf, "</u>", HTML_N_U);
  if (obuf->in_strike)
    push_tag(obuf, "</s>", HTML_N_S);
  if (obuf->in_ins)
    push_tag(obuf, "</ins>", HTML_N_INS);
  bzero(obuf->fontstat, FONTSTAT_SIZE);
}

void restore_fonteffect(struct html_feed_environ *h_env,
                        struct readbuffer *obuf) {
  if (obuf->fontstat_sp > 0)
    obuf->fontstat_sp--;
  if (obuf->fontstat_sp < FONT_STACK_SIZE)
    bcopy(obuf->fontstat_stack[obuf->fontstat_sp], obuf->fontstat,
          FONTSTAT_SIZE);
  if (obuf->in_bold)
    push_tag(obuf, "<b>", HTML_B);
  if (obuf->in_italic)
    push_tag(obuf, "<i>", HTML_I);
  if (obuf->in_under)
    push_tag(obuf, "<u>", HTML_U);
  if (obuf->in_strike)
    push_tag(obuf, "<s>", HTML_S);
  if (obuf->in_ins)
    push_tag(obuf, "<ins>", HTML_INS);
}

static Str *process_title(struct HtmlTag *tag) {
  if (pre_title)
    return NULL;
  cur_title = Strnew();
  return NULL;
}

static Str *process_n_title(struct HtmlTag *tag) {
  Str *tmp;

  if (pre_title)
    return NULL;
  if (!cur_title)
    return NULL;
  Strremovefirstspaces(cur_title);
  Strremovetrailingspaces(cur_title);
  tmp = Strnew_m_charp("<title_alt title=\"", html_quote(cur_title->ptr), "\">",
                       NULL);
  pre_title = cur_title;
  cur_title = NULL;
  return tmp;
}

static void feed_title(const char *str) {
  if (pre_title)
    return;
  if (!cur_title)
    return;
  while (*str) {
    if (*str == '&')
      Strcat_charp(cur_title, getescapecmd(&str));
    else if (*str == '\n' || *str == '\r') {
      Strcat_char(cur_title, ' ');
      str++;
    } else
      Strcat_char(cur_title, *(str++));
  }
}

Str *process_img(struct HtmlTag *tag, int width) {
  const char *p, *q, *r, *r2 = NULL, *s, *t;
  int w, i, nw, n;
  int pre_int = FALSE, ext_pre_int = FALSE;
  Str *tmp = Strnew();

  if (!parsedtag_get_value(tag, ATTR_SRC, &p))
    return tmp;
  p = url_quote(remove_space(p));
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
  if (parsedtag_exists(tag, ATTR_PRE_INT))
    ext_pre_int = TRUE;

  tmp = Strnew_size(128);
  if (r) {
    Str *tmp2;
    r2 = strchr(r, '#');
    s = "<form_int method=internal action=map>";
    tmp2 = process_form(parse_tag(&s, TRUE));
    if (tmp2)
      Strcat(tmp, tmp2);
    Strcat(tmp, Sprintf("<input_alt fid=\"%d\" "
                        "type=hidden name=link value=\"",
                        cur_form_id));
    Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
    Strcat(tmp, Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                        "type=submit no_effect=true>",
                        cur_hseq++, cur_form_id));
  }
  {
    if (w < 0)
      w = 12 * pixel_per_char;
    nw = w ? (int)((w - 1) / pixel_per_char + 1) : 1;
    if (r) {
      Strcat_charp(tmp, "<pre_int>");
      pre_int = TRUE;
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
            pre_int = TRUE;
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
        pre_int = TRUE;
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
    process_n_form();
  }
  return tmp;
}

Str *process_anchor(struct HtmlTag *tag, const char *tagbuf) {
  if (parsedtag_need_reconstruct(tag)) {
    parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", cur_hseq++)->ptr);
    return parsedtag2str(tag);
  } else {
    Str *tmp = Sprintf("<a hseq=\"%d\"", cur_hseq++);
    Strcat_charp(tmp, tagbuf + 2);
    return tmp;
  }
}

Str *process_input(struct HtmlTag *tag) {
  int i = 20, v, x, y, z, iw, ih, size = 20;
  const char *q, *p, *r, *p2, *s;
  Str *tmp = NULL;
  auto qq = "";
  int qlen = 0;

  if (cur_form_id < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
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
  x = parsedtag_exists(tag, ATTR_CHECKED);
  y = parsedtag_exists(tag, ATTR_ACCEPT);
  z = parsedtag_exists(tag, ATTR_READONLY);

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
      Strcat(tmp, getLinkNumberStr(0));
    Strcat_char(tmp, '[');
    break;
  case FORM_INPUT_RADIO:
    if (displayLinkNumber)
      Strcat(tmp, getLinkNumberStr(0));
    Strcat_char(tmp, '(');
  }
  Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      cur_hseq++, cur_form_id, html_quote(p), html_quote(r),
                      size, i, qq));
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
        Strcat(tmp, getLinkNumberStr(-1));
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

Str *process_button(struct HtmlTag *tag) {
  Str *tmp = NULL;
  const char *p, *q, *r, *qq = "";
  int v;

  if (cur_form_id < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
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
  Strcat(tmp,
         Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                 "name=\"%s\" value=\"%s\">",
                 cur_hseq++, cur_form_id, html_quote(p), html_quote(r), qq));
  return tmp;
}

Str *process_n_button(void) {
  Str *tmp = Strnew();
  Strcat_charp(tmp, "</input_alt>");
  /*    Strcat_charp(tmp, "</pre_int>"); */
  return tmp;
}

Str *process_select(struct HtmlTag *tag) {
  Str *tmp = NULL;

  if (cur_form_id < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
  }

  auto p = "";
  parsedtag_get_value(tag, ATTR_NAME, &p);
  cur_select = Strnew_charp(p);
  select_is_multiple = parsedtag_exists(tag, ATTR_MULTIPLE);

  select_str = Strnew();
  cur_option = NULL;
  cur_status = R_ST_NORMAL;
  n_selectitem = 0;
  return tmp;
}

Str *process_n_select(void) {
  if (cur_select == NULL)
    return NULL;
  process_option();
  Strcat_charp(select_str, "<br>");
  cur_select = NULL;
  n_selectitem = 0;
  return select_str;
}

void feed_select(const char *str) {
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
      if (!(tag = parse_tag(&p, FALSE)))
        continue;
      switch (tag->tagid) {
      case HTML_OPTION:
        process_option();
        cur_option = Strnew();
        if (parsedtag_get_value(tag, ATTR_VALUE, &q))
          cur_option_value = Strnew_charp(q);
        else
          cur_option_value = NULL;
        if (parsedtag_get_value(tag, ATTR_LABEL, &q))
          cur_option_label = Strnew_charp(q);
        else
          cur_option_label = NULL;
        cur_option_selected = parsedtag_exists(tag, ATTR_SELECTED);
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

void process_option(void) {
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
                             begin_char, cur_hseq++, cur_form_id,
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

Str *process_textarea(struct HtmlTag *tag, int width) {
  Str *tmp = NULL;
  const char *p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

  if (cur_form_id < 0) {
    auto s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
  }

  p = "";
  parsedtag_get_value(tag, ATTR_NAME, &p);
  cur_textarea = Strnew_charp(p);
  cur_textarea_size = 20;
  if (parsedtag_get_value(tag, ATTR_COLS, &p)) {
    cur_textarea_size = atoi(p);
    if (strlen(p) > 0 && p[strlen(p) - 1] == '%')
      cur_textarea_size = width * cur_textarea_size / 100 - 2;
    if (cur_textarea_size <= 0) {
      cur_textarea_size = 20;
    } else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX) {
      cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
    }
  }
  cur_textarea_rows = 1;
  if (parsedtag_get_value(tag, ATTR_ROWS, &p)) {
    cur_textarea_rows = atoi(p);
    if (cur_textarea_rows <= 0) {
      cur_textarea_rows = 1;
    } else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX) {
      cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
    }
  }
  cur_textarea_readonly = parsedtag_exists(tag, ATTR_READONLY);
  if (n_textarea >= max_textarea) {
    max_textarea *= 2;
    textarea_str = (Str **)New_Reuse(Str *, textarea_str, max_textarea);
  }
  textarea_str[n_textarea] = Strnew();
  ignore_nl_textarea = TRUE;

  return tmp;
}

Str *process_n_textarea(void) {
  Str *tmp;
  int i;

  if (cur_textarea == NULL)
    return NULL;

  tmp = Strnew();
  Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      cur_hseq, cur_form_id, html_quote(cur_textarea->ptr),
                      cur_textarea_size, cur_textarea_rows,
                      cur_textarea_rows - 1, n_textarea));
  if (cur_textarea_readonly)
    Strcat_charp(tmp, " readonly");
  Strcat_charp(tmp, "><u>");
  for (i = 0; i < cur_textarea_size; i++)
    Strcat_char(tmp, ' ');
  Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
  cur_hseq++;
  n_textarea++;
  cur_textarea = NULL;

  return tmp;
}

void feed_textarea(const char *str) {
  if (cur_textarea == NULL)
    return;
  if (ignore_nl_textarea) {
    if (*str == '\r')
      str++;
    if (*str == '\n')
      str++;
  }
  ignore_nl_textarea = FALSE;
  while (*str) {
    if (*str == '&')
      Strcat_charp(textarea_str[n_textarea], getescapecmd(&str));
    else if (*str == '\n') {
      Strcat_charp(textarea_str[n_textarea], "\r\n");
      str++;
    } else if (*str == '\r')
      str++;
    else
      Strcat_char(textarea_str[n_textarea], *(str++));
  }
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

static Str *process_form_int(struct HtmlTag *tag, int fid) {
  const char *p, *q, *r, *s, *tg, *n;

  p = "get";
  parsedtag_get_value(tag, ATTR_METHOD, &p);
  q = "!CURRENT_URL!";
  parsedtag_get_value(tag, ATTR_ACTION, &q);
  q = url_quote(remove_space(q));
  r = NULL;
  s = NULL;
  parsedtag_get_value(tag, ATTR_ENCTYPE, &s);
  tg = NULL;
  parsedtag_get_value(tag, ATTR_TARGET, &tg);
  n = NULL;
  parsedtag_get_value(tag, ATTR_NAME, &n);

  if (fid < 0) {
    form_max++;
    form_sp++;
    fid = form_max;
  } else { /* <form_int> */
    if (form_max < fid)
      form_max = fid;
    form_sp = fid;
  }
  if (forms_size == 0) {
    forms_size = INITIAL_FORM_SIZE;
    forms = (FormList **)New_N(FormList *, forms_size);
    form_stack = (int *)NewAtom_N(int, forms_size);
  }
  if (forms_size <= form_max) {
    forms_size += form_max;
    forms = (FormList **)New_Reuse(FormList *, forms, forms_size);
    form_stack = (int *)New_Reuse(int, form_stack, forms_size);
  }
  form_stack[form_sp] = fid;

  forms[fid] = newFormList(q, p, r, s, tg, n, NULL);
  return NULL;
}

Str *process_form(struct HtmlTag *tag) { return process_form_int(tag, -1); }

Str *process_n_form(void) {
  if (form_sp >= 0)
    form_sp--;
  return NULL;
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
static void process_idattr(struct readbuffer *obuf, int cmd,
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
  push_tag(obuf, idtag->ptr, HTML_NOP);
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
      close_anchor(h_env, obuf);                                               \
    }                                                                          \
  } while (0)

#define CLOSE_A                                                                \
  do {                                                                         \
    CLOSE_P;                                                                   \
    if (!(obuf->flag & RB_HTML5)) {                                            \
      close_anchor(h_env, obuf);                                               \
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

int HTMLtagproc1(struct HtmlTag *tag, struct html_feed_environ *h_env) {
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
    close_anchor(h_env, obuf);
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
    close_anchor(h_env, obuf);
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
    if (parsedtag_exists(tag, ATTR_COMPACT))
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
    close_anchor(h_env, obuf);
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
    close_anchor(h_env, obuf);
    tmp = process_hr(tag, h_env->limit, envs[h_env->envc].indent);
    HTMLlineproc1(tmp->ptr, h_env);
    set_space_to_prevchar(obuf->prevchar);
    return 1;
  case HTML_PRE:
    x = parsedtag_exists(tag, ATTR_FOR_TABLE);
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
    close_anchor(h_env, obuf);
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
    if (obuf->anchor.url)
      close_anchor(h_env, obuf);

    hseq = 0;

    if (parsedtag_get_value(tag, ATTR_HREF, &p))
      obuf->anchor.url = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
      obuf->anchor.target = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_REFERER, &p))
      obuf->anchor.referer = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
      obuf->anchor.title = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_ACCESSKEY, &p))
      obuf->anchor.accesskey = (unsigned char)*p;
    if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq))
      obuf->anchor.hseq = hseq;

    if (hseq == 0 && obuf->anchor.url) {
      obuf->anchor.hseq = cur_hseq;
      tmp = process_anchor(tag, h_env->tagbuf->ptr);
      push_tag(obuf, tmp->ptr, HTML_A);
      return 1;
    }
    return 0;
  case HTML_N_A:
    close_anchor(h_env, obuf);
    return 1;
  case HTML_IMG:
    if (parsedtag_exists(tag, ATTR_USEMAP))
      HTML5_CLOSE_A;
    tmp = process_img(tag, h_env->limit);
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
    close_anchor(h_env, obuf);
    if (obuf->table_level + 1 >= MAX_TABLE)
      break;
    obuf->table_level++;
    w = BORDER_NONE;
    /* x: cellspacing, y: cellpadding */
    x = 2;
    y = 1;
    z = 0;
    width = 0;
    if (parsedtag_exists(tag, ATTR_BORDER)) {
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
    if (parsedtag_exists(tag, ATTR_HBORDER))
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
    close_anchor(h_env, obuf);
    tmp = process_input(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_BUTTON:
    HTML5_CLOSE_A;
    tmp = process_button(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_N_BUTTON:
    tmp = process_n_button();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_SELECT:
    close_anchor(h_env, obuf);
    tmp = process_select(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    obuf->flag |= RB_INSELECT;
    obuf->end_tag = HTML_N_SELECT;
    return 1;
  case HTML_N_SELECT:
    obuf->flag &= ~RB_INSELECT;
    obuf->end_tag = 0;
    tmp = process_n_select();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_OPTION:
    /* nothing */
    return 1;
  case HTML_TEXTAREA:
    close_anchor(h_env, obuf);
    tmp = process_textarea(tag, h_env->limit);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    obuf->flag |= RB_INTXTA;
    obuf->end_tag = HTML_N_TEXTAREA;
    return 1;
  case HTML_N_TEXTAREA:
    obuf->flag &= ~RB_INTXTA;
    obuf->end_tag = 0;
    tmp = process_n_textarea();
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
    if (!parsedtag_exists(tag, ATTR_PUBLIC)) {
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

static int ex_efct(int ex) {
  int effect = 0;

  if (!ex)
    return 0;

  if (ex & PE_EX_ITALIC)
    effect |= PE_EX_ITALIC_E;

  if (ex & PE_EX_INSERT)
    effect |= PE_EX_INSERT_E;

  if (ex & PE_EX_STRIKE)
    effect |= PE_EX_STRIKE_E;

  return effect;
}

static void addLink(Buffer *buf, struct HtmlTag *tag) {
  const char *href = NULL, *title = NULL, *ctype = NULL, *rel = NULL,
             *rev = NULL;
  char type = LINK_TYPE_NONE;
  LinkList *l;

  parsedtag_get_value(tag, ATTR_HREF, &href);
  if (href)
    href = url_quote(remove_space(href));
  parsedtag_get_value(tag, ATTR_TITLE, &title);
  parsedtag_get_value(tag, ATTR_TYPE, &ctype);
  parsedtag_get_value(tag, ATTR_REL, &rel);
  if (rel != NULL) {
    /* forward link type */
    type = LINK_TYPE_REL;
    if (title == NULL)
      title = rel;
  }
  parsedtag_get_value(tag, ATTR_REV, &rev);
  if (rev != NULL) {
    /* reverse link type */
    type = LINK_TYPE_REV;
    if (title == NULL)
      title = rev;
  }

  l = (LinkList *)New(LinkList);
  l->url = href;
  l->title = title;
  l->ctype = ctype;
  l->type = type;
  l->next = NULL;
  if (buf->linklist) {
    LinkList *i;
    for (i = buf->linklist; i->next; i = i->next)
      ;
    i->next = l;
  } else
    buf->linklist = l;
}

// static input_stream *_file_lp2;
// static Str *file_feed(void) {
//   Str *s;
//   s = StrISgets(_file_lp2);
//   if (s && s->length == 0) {
//     ISclose(_file_lp2);
//     return NULL;
//   }
//   return s;
// }

static void proc_escape(struct readbuffer *obuf, const char **str_return) {
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
void HTMLlineproc0(const char *line, struct html_feed_environ *h_env,
                   int internal) {
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
    int is_tag = FALSE;
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
      str = Strdup(h_env->tagbuf)->ptr;
      if (*str == '<') {
        if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
          is_tag = TRUE;
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
        if ((tag = parse_tag(&p, internal))) {
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
        feed_select(str);
        continue;
      }
      if (is_tag) {
        if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<'))) {
          str = Strnew_charp_n(str, p - str)->ptr;
          line = Strnew_m_charp(p, line, NULL)->ptr;
        }
        is_tag = FALSE;
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
      switch (feed_table(tbl, str, tbl_mode, tbl_width, internal)) {
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
          feed_table(tbl, str, tbl_mode, tbl_width, TRUE);
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
        renderTable(tbl, tbl_width, h_env);
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
      if ((tag = parse_tag(&str, internal)))
        cmd = tag->tagid;
      else
        continue;
      /* process tags */
      if (HTMLtagproc1(tag, h_env) == 0) {
        /* preserve the tag for second-stage processing */
        if (parsedtag_need_reconstruct(tag))
          h_env->tagbuf = parsedtag2str(tag);
        push_tag(obuf, h_env->tagbuf->ptr, cmd);
      }
#ifdef ID_EXT
      else {
        process_idattr(obuf, cmd, tag);
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

static void print_internal_information(struct html_feed_environ *henv) {
  int i;
  Str *s;
  TextLineList *tl = newTextLineList();

  s = Strnew_charp("<internal>");
  pushTextLine(tl, newTextLine(s, 0));
  if (henv->title) {
    s = Strnew_m_charp("<title_alt title=\"", html_quote(henv->title), "\">",
                       NULL);
    pushTextLine(tl, newTextLine(s, 0));
  }
  if (n_textarea > 0) {
    for (i = 0; i < n_textarea; i++) {
      s = Sprintf("<textarea_int textareanumber=%d>", i);
      pushTextLine(tl, newTextLine(s, 0));
      s = Strnew_charp(html_quote(textarea_str[i]->ptr));
      Strcat_charp(s, "</textarea_int>");
      pushTextLine(tl, newTextLine(s, 0));
    }
  }
  s = Strnew_charp("</internal>");
  pushTextLine(tl, newTextLine(s, 0));

  if (henv->buf)
    appendTextLineList(henv->buf, tl);
  else if (henv->f) {
    TextLineListItem *p;
    for (p = tl->first; p; p = p->next)
      fprintf(henv->f, "%s\n", p->ptr->line->ptr);
  }
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

#define PPUSH(p, c)                                                            \
  {                                                                            \
    outp[pos] = (p);                                                           \
    outc[pos] = (c);                                                           \
    pos++;                                                                     \
  }
#define PSIZE                                                                  \
  if (out_size <= pos + 1) {                                                   \
    out_size = pos * 3 / 2;                                                    \
    outc = (char *)New_Reuse(char, outc, out_size);                            \
    outp = (Lineprop *)New_Reuse(Lineprop, outp, out_size);                    \
  }

static void HTMLlineproc2body(Buffer *buf, Str *(*feed)(), int llimit) {
  static char *outc = NULL;
  static Lineprop *outp = NULL;
  static int out_size = 0;
  Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
  const char *p, *q, *r, *s, *t, *str;
  Lineprop mode, effect, ex_effect;
  int pos;
  int nlines;
#ifdef DEBUG
  FILE *debug = NULL;
#endif
  const char *id = NULL;
  int hseq, form_id;
  Str *line;
  const char *endp;
  char symbol = '\0';
  int internal = 0;
  Anchor **a_textarea = NULL;

  if (out_size == 0) {
    out_size = LINELEN;
    outc = (char *)NewAtom_N(char, out_size);
    outp = (Lineprop *)NewAtom_N(Lineprop, out_size);
  }

  n_textarea = -1;
  if (!max_textarea) { /* halfload */
    max_textarea = MAX_TEXTAREA;
    textarea_str = (Str **)New_N(Str *, max_textarea);
    a_textarea = (Anchor **)New_N(Anchor *, max_textarea);
  }

#ifdef DEBUG
  if (w3m_debug)
    debug = fopen("zzzerr", "a");
#endif

  effect = 0;
  ex_effect = 0;
  nlines = 0;
  while ((line = feed()) != NULL) {
#ifdef DEBUG
    if (w3m_debug) {
      Strfputs(line, debug);
      fputc('\n', debug);
    }
#endif
    if (n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
      Strcat(textarea_str[n_textarea], line);
      continue;
    }
  proc_again:
    if (++nlines == llimit)
      break;
    pos = 0;
#ifdef ENABLE_REMOVE_TRAILINGSPACES
    Strremovetrailingspaces(line);
#endif
    str = line->ptr;
    endp = str + line->length;
    while (str < endp) {
      PSIZE;
      mode = get_mctype(str);
      if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<') {
        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), SYMBOL_BASE + symbol);
        str += symbol_width;
      } else if (mode == PC_CTRL || IS_INTSPACE(*str)) {
        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
        str++;
      } else if (*str != '<' && *str != '&') {
        int len = get_mclen(str);
        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
        if (--len) {
          mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
          while (len--) {
            PSIZE;
            PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
          }
        }
      } else if (*str == '&') {
        /*
         * & escape processing
         */
        p = getescapecmd(&str);
        while (*p) {
          PSIZE;
          mode = get_mctype(p);
          if (mode == PC_CTRL || IS_INTSPACE(*str)) {
            PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
            p++;
          } else {
            PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
          }
        }
      } else {
        /* tag processing */
        struct HtmlTag *tag;
        if (!(tag = parse_tag(&str, TRUE)))
          continue;
        switch (tag->tagid) {
        case HTML_B:
          effect |= PE_BOLD;
          break;
        case HTML_N_B:
          effect &= ~PE_BOLD;
          break;
        case HTML_I:
          ex_effect |= PE_EX_ITALIC;
          break;
        case HTML_N_I:
          ex_effect &= ~PE_EX_ITALIC;
          break;
        case HTML_INS:
          ex_effect |= PE_EX_INSERT;
          break;
        case HTML_N_INS:
          ex_effect &= ~PE_EX_INSERT;
          break;
        case HTML_U:
          effect |= PE_UNDER;
          break;
        case HTML_N_U:
          effect &= ~PE_UNDER;
          break;
        case HTML_S:
          ex_effect |= PE_EX_STRIKE;
          break;
        case HTML_N_S:
          ex_effect &= ~PE_EX_STRIKE;
          break;
        case HTML_A:
          p = r = s = NULL;
          q = buf->baseTarget;
          t = "";
          hseq = 0;
          id = NULL;
          if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
            id = url_quote_conv(id, name_charset);
            registerName(buf, id, currentLn(buf), pos);
          }
          if (parsedtag_get_value(tag, ATTR_HREF, &p))
            p = url_quote(remove_space(p));
          if (parsedtag_get_value(tag, ATTR_TARGET, &q))
            q = url_quote_conv(q, buf->document_charset);
          if (parsedtag_get_value(tag, ATTR_REFERER, &r))
            r = url_quote(r);
          parsedtag_get_value(tag, ATTR_TITLE, &s);
          parsedtag_get_value(tag, ATTR_ACCESSKEY, &t);
          parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
          if (hseq > 0)
            buf->hmarklist =
                putHmarker(buf->hmarklist, currentLn(buf), pos, hseq - 1);
          else if (hseq < 0) {
            int h = -hseq - 1;
            if (buf->hmarklist && h < buf->hmarklist->nmark &&
                buf->hmarklist->marks[h].invalid) {
              buf->hmarklist->marks[h].pos = pos;
              buf->hmarklist->marks[h].line = currentLn(buf);
              buf->hmarklist->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }
          if (p) {
            effect |= PE_ANCHOR;
            a_href = registerHref(buf, p, q, r, s, *t, currentLn(buf), pos);
            a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
            a_href->slave = (hseq > 0) ? FALSE : TRUE;
          }
          break;
        case HTML_N_A:
          effect &= ~PE_ANCHOR;
          if (a_href) {
            a_href->end.line = currentLn(buf);
            a_href->end.pos = pos;
            if (a_href->start.line == a_href->end.line &&
                a_href->start.pos == a_href->end.pos) {
              if (buf->hmarklist && a_href->hseq >= 0 &&
                  a_href->hseq < buf->hmarklist->nmark)
                buf->hmarklist->marks[a_href->hseq].invalid = 1;
              a_href->hseq = -1;
            }
            a_href = NULL;
          }
          break;

        case HTML_LINK:
          addLink(buf, tag);
          break;

        case HTML_IMG_ALT:
          if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
            s = NULL;
            parsedtag_get_value(tag, ATTR_TITLE, &s);
            p = url_quote_conv(remove_space(p), buf->document_charset);
            a_img = registerImg(buf, p, s, currentLn(buf), pos);
          }
          effect |= PE_IMAGE;
          break;
        case HTML_N_IMG_ALT:
          effect &= ~PE_IMAGE;
          if (a_img) {
            a_img->end.line = currentLn(buf);
            a_img->end.pos = pos;
          }
          a_img = NULL;
          break;
        case HTML_INPUT_ALT: {
          FormList *form;
          int top = 0, bottom = 0;
          int textareanumber = -1;
          hseq = 0;
          form_id = -1;

          parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
          parsedtag_get_value(tag, ATTR_FID, &form_id);
          parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
          parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
          if (form_id < 0 || form_id > form_max || forms == NULL ||
              forms[form_id] == NULL)
            break; /* outside of <form>..</form> */
          form = forms[form_id];
          if (hseq > 0) {
            int hpos = pos;
            if (*str == '[')
              hpos++;
            buf->hmarklist =
                putHmarker(buf->hmarklist, currentLn(buf), hpos, hseq - 1);
          } else if (hseq < 0) {
            int h = -hseq - 1;
            int hpos = pos;
            if (*str == '[')
              hpos++;
            if (buf->hmarklist && h < buf->hmarklist->nmark &&
                buf->hmarklist->marks[h].invalid) {
              buf->hmarklist->marks[h].pos = hpos;
              buf->hmarklist->marks[h].line = currentLn(buf);
              buf->hmarklist->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }

          if (!form->target)
            form->target = buf->baseTarget;
          if (a_textarea &&
              parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &textareanumber)) {
            if (textareanumber >= max_textarea) {
              max_textarea = 2 * textareanumber;
              textarea_str =
                  (Str **)New_Reuse(Str *, textarea_str, max_textarea);
              a_textarea =
                  (Anchor **)New_Reuse(Anchor *, a_textarea, max_textarea);
            }
          }
          a_form = registerForm(buf, form, tag, currentLn(buf), pos);
          if (a_textarea && textareanumber >= 0)
            a_textarea[textareanumber] = a_form;
          if (a_form) {
            a_form->hseq = hseq - 1;
            a_form->y = currentLn(buf) - top;
            a_form->rows = 1 + top + bottom;
            if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
              effect |= PE_FORM;
            break;
          }
        }
        case HTML_N_INPUT_ALT:
          effect &= ~PE_FORM;
          if (a_form) {
            a_form->end.line = currentLn(buf);
            a_form->end.pos = pos;
            if (a_form->start.line == a_form->end.line &&
                a_form->start.pos == a_form->end.pos)
              a_form->hseq = -1;
          }
          a_form = NULL;
          break;
        case HTML_MAP:
          // if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
          //   MapList *m = (MapList *)New(MapList);
          //   m->name = Strnew_charp(p);
          //   m->area = newGeneralList();
          //   m->next = buf->maplist;
          //   buf->maplist = m;
          // }
          break;
        case HTML_N_MAP:
          /* nothing to do */
          break;
        case HTML_AREA:
          // if (buf->maplist == NULL) /* outside of <map>..</map> */
          //   break;
          // if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
          //   MapArea *a;
          //   p = url_encode(remove_space(p), base, buf->document_charset);
          //   t = NULL;
          //   parsedtag_get_value(tag, ATTR_TARGET, &t);
          //   q = "";
          //   parsedtag_get_value(tag, ATTR_ALT, &q);
          //   r = NULL;
          //   s = NULL;
          //   a = newMapArea(p, t, q, r, s);
          //   pushValue(buf->maplist->area, (void *)a);
          // }
          break;
        case HTML_FRAMESET:
          // frameset_sp++;
          // if (frameset_sp >= FRAMESTACK_SIZE)
          //   break;
          // frameset_s[frameset_sp] = newFrameSet(tag);
          // if (frameset_s[frameset_sp] == NULL)
          //   break;
          break;
        case HTML_N_FRAMESET:
          // if (frameset_sp >= 0)
          //   frameset_sp--;
          break;
        case HTML_FRAME:
          // if (frameset_sp >= 0 && frameset_sp < FRAMESTACK_SIZE) {
          // }
          break;
        case HTML_BASE:
          if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
            p = url_quote(remove_space(p));
            if (!buf->baseURL)
              buf->baseURL = (ParsedURL *)New(ParsedURL);
            parseURL2(p, buf->baseURL, &buf->currentURL);
          }
          if (parsedtag_get_value(tag, ATTR_TARGET, &p))
            buf->baseTarget = (char *)url_quote_conv(p, buf->document_charset);
          break;
        case HTML_META:
          p = q = NULL;
          parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
          parsedtag_get_value(tag, ATTR_CONTENT, &q);
          if (p && q && !strcasecmp(p, "refresh") && MetaRefresh) {
            Str *tmp = NULL;
            int refresh_interval = getMetaRefreshParam(q, &tmp);
            if (tmp) {
              p = url_quote(remove_space(tmp->ptr));
              buf->event =
                  setAlarmEvent(buf->event, refresh_interval, AL_IMPLICIT_ONCE,
                                FUNCNAME_gorURL, (void *)p);
            } else if (refresh_interval > 0)
              buf->event = setAlarmEvent(buf->event, refresh_interval,
                                         AL_IMPLICIT, FUNCNAME_reload, NULL);
          }
          break;
        case HTML_INTERNAL:
          internal = HTML_INTERNAL;
          break;
        case HTML_N_INTERNAL:
          internal = HTML_N_INTERNAL;
          break;
        case HTML_FORM_INT:
          if (parsedtag_get_value(tag, ATTR_FID, &form_id))
            process_form_int(tag, form_id);
          break;
        case HTML_TEXTAREA_INT:
          if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &n_textarea) &&
              n_textarea >= 0 && n_textarea < max_textarea) {
            textarea_str[n_textarea] = Strnew();
          } else
            n_textarea = -1;
          break;
        case HTML_N_TEXTAREA_INT:
          if (a_textarea && n_textarea >= 0) {
            FormItemList *item = (FormItemList *)a_textarea[n_textarea]->url;
            item->init_value = item->value = textarea_str[n_textarea];
          }
          break;
        case HTML_TITLE_ALT:
          if (parsedtag_get_value(tag, ATTR_TITLE, &p))
            buf->buffername = html_unquote(p);
          break;
        case HTML_SYMBOL:
          effect |= PC_SYMBOL;
          if (parsedtag_get_value(tag, ATTR_TYPE, &p))
            symbol = (char)atoi(p);
          break;
        case HTML_N_SYMBOL:
          effect &= ~PC_SYMBOL;
          break;

        default:
          break;
        }
#ifdef ID_EXT
        id = NULL;
        if (parsedtag_get_value(tag, ATTR_ID, &id)) {
          id = url_quote_conv(id, name_charset);
          registerName(buf, id, currentLn(buf), pos);
        }
#endif /* ID_EXT */
      }
    }
    /* end of processing for one line */
    if (!internal)
      addnewline(buf, outc, outp, NULL, pos, -1, nlines);
    if (internal == HTML_N_INTERNAL)
      internal = 0;
    if (str != endp) {
      line = Strsubstr(line, str - line->ptr, endp - str);
      goto proc_again;
    }
  }
#ifdef DEBUG
  if (w3m_debug)
    fclose(debug);
#endif
  for (form_id = 1; form_id <= form_max; form_id++)
    if (forms[form_id])
      forms[form_id]->next = forms[form_id - 1];
  buf->formlist = (form_max >= 0) ? forms[form_max] : NULL;
  if (n_textarea)
    addMultirowsForm(buf, buf->formitem);
}
void HTMLlineproc2(Buffer *buf, TextLineList *tl) {
  _tl_lp2 = tl->first;
  HTMLlineproc2body(buf, textlist_feed, -1);
}

// static void HTMLlineproc3(Buffer *buf, input_stream *stream) {
//   _file_lp2 = stream;
//   HTMLlineproc2body(buf, file_feed, -1);
// }

void loadHTMLstream(UrlStream *f, Buffer *newBuf, FILE *src, int internal) {
  struct environment envs[MAX_ENV_LEVEL];
  long long linelen = 0;
  long long trbyte = 0;
  Str *lineBuf2 = Strnew();
  struct html_feed_environ htmlenv1;
  struct readbuffer obuf;
  MySignalHandler prevtrap = NULL;

  symbol_width = symbol_width0 = 1;

  cur_title = NULL;
  pre_title = NULL;
  n_textarea = 0;
  cur_textarea = NULL;
  max_textarea = MAX_TEXTAREA;
  textarea_str = (Str **)New_N(Str *, max_textarea);
  cur_select = NULL;
  form_sp = -1;
  form_max = -1;
  forms_size = 0;
  forms = NULL;
  cur_hseq = 1;

  init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);

  htmlenv1.buf = newTextLineList();

  if (SETJMP(AbortLoading) != 0) {
    HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1);
    goto phase2;
  }
  TRAP_ON;

  if (IStype(f->stream) != IST_ENCODED)
    f->stream = newEncodedStream(f->stream, f->encoding);
  while ((lineBuf2 = StrmyUFgets(f)) && lineBuf2->length) {
    if (src)
      Strfputs(lineBuf2, src);
    linelen += lineBuf2->length;
    // showProgress(&linelen, &trbyte);
    /*
     * if (frame_source)
     * continue;
     */
    lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);
    HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
  }
  if (obuf.status != R_ST_NORMAL) {
    HTMLlineproc0("\n", &htmlenv1, internal);
  }
  obuf.status = R_ST_NORMAL;
  completeHTMLstream(&htmlenv1, &obuf);
  flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
  if (htmlenv1.title)
    newBuf->buffername = htmlenv1.title;

  if (w3m_backend) {
    TRAP_OFF;
    print_internal_information(&htmlenv1);
    backend_halfdump_buf = htmlenv1.buf;
    return;
  }
phase2:
  newBuf->trbyte = trbyte + linelen;
  TRAP_OFF;
  HTMLlineproc2(newBuf, htmlenv1.buf);
}

/*
 * loadHTMLBuffer: read file and make new buffer
 */
Buffer *loadHTMLBuffer(UrlStream *f, Buffer *newBuf) {
  FILE *src = NULL;
  Str *tmp;

  if (newBuf == NULL)
    newBuf = new Buffer(INIT_BUFFER_WIDTH);
  if (newBuf->sourcefile == NULL &&
      (f->schema != SCM_LOCAL || newBuf->mailcap)) {
    tmp = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmp->ptr, "w");
    if (src)
      newBuf->sourcefile = tmp->ptr;
  }

  loadHTMLstream(f, newBuf, src, newBuf->bufferprop);

  newBuf->topLine = newBuf->firstLine;
  newBuf->lastLine = newBuf->currentLine;
  newBuf->currentLine = newBuf->firstLine;
  if (n_textarea)
    formResetBuffer(newBuf, newBuf->formitem);
  if (src)
    fclose(src);

  return newBuf;
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

void completeHTMLstream(struct html_feed_environ *h_env,
                        struct readbuffer *obuf) {
  close_anchor(h_env, obuf);
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
    HTMLlineproc1("</textarea>", h_env);
  /* for unbalanced select tag */
  if (obuf->flag & RB_INSELECT)
    HTMLlineproc1("</select>", h_env);
  if (obuf->flag & RB_TITLE)
    HTMLlineproc1("</title>", h_env);

  /* for unbalanced table tag */
  if (obuf->table_level >= MAX_TABLE)
    obuf->table_level = MAX_TABLE - 1;

  while (obuf->table_level >= 0) {
    int tmp = obuf->table_level;
    table_mode[obuf->table_level].pre_mode &=
        ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
    HTMLlineproc1("</table>", h_env);
    if (obuf->table_level >= tmp)
      break;
  }
}

/*
 * loadHTMLString: read string and make new buffer
 */
Buffer *loadHTMLString(Str *page) {
  MySignalHandler prevtrap = NULL;

  UrlStream f;
  init_stream(&f, SCM_LOCAL, newStrStream(page));

  auto newBuf = new Buffer(INIT_BUFFER_WIDTH);
  if (SETJMP(AbortLoading) != 0) {
    TRAP_OFF;
    discardBuffer(newBuf);
    UFclose(&f);
    return NULL;
  }
  TRAP_ON;

  loadHTMLstream(&f, newBuf, NULL, TRUE);

  TRAP_OFF;
  UFclose(&f);
  newBuf->topLine = newBuf->firstLine;
  newBuf->lastLine = newBuf->currentLine;
  newBuf->currentLine = newBuf->firstLine;
  newBuf->type = "text/html";
  newBuf->real_type = newBuf->type;
  if (n_textarea)
    formResetBuffer(newBuf, newBuf->formitem);
  return newBuf;
}
