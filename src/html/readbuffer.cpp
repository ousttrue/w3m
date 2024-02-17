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
#include "form.h"
#include "textlist.h"
#include "terms.h"
#include "istream.h"
#include "html_tag.h"
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

  symbol_width = symbol_width0 = 1;

  init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, INIT_BUFFER_WIDTH(),
            0);

  htmlenv1.buf = newTextLineList();

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

  res->trbyte = trbyte + linelen;
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

  obuf->bp.anchor = obuf->anchor;
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
