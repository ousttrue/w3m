#include "readbuffer.h"
#include "html_feed_env.h"
#include "etc.h"
#include "html/form_item.h"
// #include "app.h"
#include "buffer.h"
#include "line_layout.h"
#include "http_response.h"
#include "symbol.h"
#include "Str.h"
#include "myctype.h"
#include "ctrlcode.h"
#include "table.h"
#include "html.h"
#include "textline.h"
#include "html_tag.h"
#include "proc.h"
#include "stringtoken.h"
#include <math.h>
#include <string_view>
#include "html_parser.h"

int squeezeBlankLine = false;
bool pseudoInlines = true;
bool ignore_null_img_alt = true;
int pixel_per_char_i = static_cast<int>(DEFAULT_PIXEL_PER_CHAR);
bool displayLinkNumber = false;
bool DisableCenter = false;
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

int next_status(char c, ReadBufferStatus *status) {
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

  case R_ST_EOL:
    break;
  }
  /* notreached */
  return 0;
}

int read_token(Str *buf, const char **instr, ReadBufferStatus *status, int pre,
               int append) {
  if (!append)
    Strclear(buf);
  if (**instr == '\0')
    return 0;

  ReadBufferStatus prev_status;
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

    case R_ST_EOL:
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

readbuffer::readbuffer() {
  this->line = Strnew();
  this->prevchar = Strnew_size(8);
  set_space_to_prevchar(this->prevchar);
  this->flag = RB_IGNORE_P;
  this->status = R_ST_NORMAL;
  this->prev_ctype = PC_ASCII;
  this->bp.init_flag = 1;
  this->set_breakpoint(0);
}

void loadHTMLstream(const std::shared_ptr<LineLayout> &layout, int width,
                    const Url &currentURL, std::string_view body,
                    bool internal) {

  symbol_width = 1;

  html_feed_environ htmlenv1(MAX_ENV_LEVEL, width, 0);

  htmlenv1.buf = GeneralList<TextLine>::newGeneralList();

  auto begin = body.begin();
  while (begin != body.end()) {
    auto it = begin;
    for (; it != body.end(); ++it) {
      if (*it == '\n') {
        break;
      }
    }
    std::string_view l(begin, it);
    begin = it;
    if (begin != body.end()) {
      ++begin;
    }

    auto line = cleanup_line(l, HTML_MODE);
    htmlenv1.parseLine(line, internal);
  }

  if (htmlenv1.obuf.status != R_ST_NORMAL) {
    htmlenv1.parseLine("\n", internal);
  }

  htmlenv1.obuf.status = R_ST_NORMAL;
  htmlenv1.completeHTMLstream();
  htmlenv1.flushline(0, 2, htmlenv1.limit);

  //
  // render ?
  //
  htmlenv1.render(layout, currentURL);
}

std::string cleanup_line(std::string_view _s, CleanupMode mode) {
  std::string s{_s.begin(), _s.end()};
  if (s.empty()) {
    return s;
  }
  if (mode == RAW_MODE) {
    return s;
  }

  if (s.size() >= 2 && s[s.size() - 2] == '\r' && s[s.size() - 1] == '\n') {
    s.pop_back();
    s.pop_back();
    s += '\n';
  } else if (s.back() == '\r') {
    s[s.size() - 1] = '\n';
  } else if (s.back() != '\n') {
    s += '\n';
  }

  if (mode != PAGER_MODE) {
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '\0') {
        s[i] = ' ';
      }
    }
  }
  return s;
}

/*
 * Check character type
 */
#define LINELEN 256 /* Initial line length */

static Str *checkType(Str *s, Lineprop **oprop) {
  static Lineprop *prop_buffer = NULL;
  static int prop_size = 0;

  char *str = s->ptr;
  char *endp = &s->ptr[s->length];
  char *bs = NULL;

  if (prop_size < s->length) {
    prop_size = (s->length > LINELEN) ? s->length : LINELEN;
    prop_buffer = (Lineprop *)New_Reuse(Lineprop, prop_buffer, prop_size);
  }
  auto prop = prop_buffer;

  bool do_copy = false;
  if (!do_copy) {
    for (; str < endp && IS_ASCII(*str); str++) {
      *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
    }
  }

  Lineprop effect = PE_NORMAL;
  while (str < endp) {
    if (prop - prop_buffer >= prop_size)
      break;
    if (bs != NULL) {
      if (str == bs - 1 && *str == '_') {
        str += 2;
        effect = PE_UNDER;
        if (str < endp)
          bs = (char *)memchr(str, '\b', endp - str);
        continue;
      } else if (str == bs) {
        if (*(str + 1) == '_') {
          if (s->length) {
            str += 2;
            *(prop - 1) |= PE_UNDER;
          } else {
            str++;
          }
        } else {
          if (s->length) {
            if (*(str - 1) == *(str + 1)) {
              *(prop - 1) |= PE_BOLD;
              str += 2;
            } else {
              Strshrink(s, 1);
              prop--;
              str++;
            }
          } else {
            str++;
          }
        }
        if (str < endp)
          bs = (char *)memchr(str, '\b', endp - str);
        continue;
      }
    }

    auto mode = get_mctype(str) | effect;
    *(prop++) = mode;
    {
      if (do_copy)
        Strcat_char(s, (char)*str);
      str++;
    }
    effect = PE_NORMAL;
  }
  *oprop = prop_buffer;
  return s;
}

void loadBuffer(const std::shared_ptr<LineLayout> &layout, HttpResponse *res,
                std::string_view page) {
  layout->clearBuffer();
  auto nlines = 0;
  // auto stream = newStrStream(page);
  char pre_lbuf = '\0';

  auto begin = page.begin();
  while (begin != page.end()) {
    auto it = begin;
    for (; it != page.end(); ++it) {
      if (*it == '\n') {
        break;
      }
    }
    std::string_view l(begin, it);
    begin = it;
    if (begin != page.end()) {
      ++begin;
    }
    auto line = cleanup_line(l, PAGER_MODE);

    if (squeezeBlankLine) {
      if (line[0] == '\n' && pre_lbuf == '\n') {
        ++nlines;
        continue;
      }
      pre_lbuf = line[0];
    }
    ++nlines;
    auto lineBuf2 = Strnew(line);
    Strchop(lineBuf2);

    Lineprop *propBuffer = nullptr;
    lineBuf2 = checkType(lineBuf2, &propBuffer);
    layout->data.addnewline(lineBuf2->ptr, propBuffer, lineBuf2->length);
  }
  // layout->_scroll.row = 0;
  // layout->_cursor.row = 0;
  res->type = "text/plain";
}

// std::shared_ptr<Buffer>
// loadHTMLString(const std::shared_ptr<LineLayout> &layout, int width,
//                const Url &currentUrl, std::string_view html) {
//   auto newBuf = Buffer::create();
//   newBuf->layout = loadHTMLstream(layout, width, currentUrl, html, true);
//   newBuf->res->raw = {html.begin(), html.end()};
//   newBuf->res->type = "text/html";
//   return newBuf;
// }

#define SHELLBUFFERNAME "*Shellout*"
std::shared_ptr<Buffer> getshell(const char *cmd) {
#ifdef _MSC_VER
  return {};
#else
  if (cmd == nullptr || *cmd == '\0') {
    return nullptr;
  }

  auto f = popen(cmd, "r");
  if (f == nullptr) {
    return nullptr;
  }

  auto buf = Buffer::create();
  // UrlStream uf(SCM_UNKNOWN, newFileStream(f, pclose));
  // TODO:
  // loadBuffer(buf->res.get(), &buf->layout);
  buf->res->filename = cmd;
  buf->layout.data.title = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  return buf;
#endif
}

void readbuffer::set_breakpoint(int tag_length) {
  this->bp.len = this->line->length;
  this->bp.pos = this->pos;
  this->bp.tlen = tag_length;
  this->bp.flag = this->flag;
  this->bp.flag &= ~RB_FILL;
  this->bp.top_margin = this->top_margin;
  this->bp.bottom_margin = this->bottom_margin;

  if (!this->bp.init_flag)
    return;

  this->bp.img_alt = this->img_alt;
  this->bp.input_alt = this->input_alt;
  this->bp.fontstat = this->fontstat;
  this->bp.nobr_level = this->nobr_level;
  this->bp.prev_ctype = this->prev_ctype;
  this->bp.init_flag = 0;
}

void readbuffer::push_link(int cmd, int offset, int pos) {
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

void readbuffer::append_tags() {
  int len = this->line->length;
  bool set_bp = false;
  for (int i = 0; i < this->tag_sp; i++) {
    switch (this->tag_stack[i]->cmd) {
    case HTML_A:
    case HTML_IMG_ALT:
    case HTML_B:
    case HTML_U:
    case HTML_I:
    case HTML_S:
      this->push_link(this->tag_stack[i]->cmd, this->line->length, this->pos);
      break;
    }
    Strcat_charp(this->line, this->tag_stack[i]->cmdname);
    switch (this->tag_stack[i]->cmd) {
    case HTML_NOBR:
      if (this->nobr_level > 1)
        break;
    case HTML_WBR:
      set_bp = 1;
      break;
    }
  }
  this->tag_sp = 0;
  if (set_bp) {
    this->set_breakpoint(this->line->length - len);
  }
}

char *readbuffer::has_hidden_link(int cmd) const {
  Str *line = this->line;
  if (Strlastchar(line) != '>')
    return nullptr;

  struct link_stack *p;
  for (p = link_stack; p; p = p->next)
    if (p->cmd == cmd)
      break;
  if (!p)
    return nullptr;

  if (this->pos == p->pos)
    return line->ptr + p->offset;

  return nullptr;
}

void readbuffer::passthrough(HtmlParser *parser, char *str, int back) {
  int cmd;
  Str *tok = Strnew();

  if (back) {
    Str *str_save = Strnew_charp(str);
    Strshrink(this->line, this->line->ptr + this->line->length - str);
    str = str_save->ptr;
  }
  while (*str) {
    auto str_bak = str;
    stringtoken st(str);
    auto token = st.sloppy_parse_line();
    str = (char *)st.ptr();
    if (token) {
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
        Strcat(tok, *token);
        parser->push_tag(this, tok->ptr, cmd);
        Strclear(tok);
      }
    } else {
      parser->push_nchars(this, 0, str_bak, str - str_bak, this->prev_ctype);
    }
  }
}
