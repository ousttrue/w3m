#include "readbuffer.h"
#include "option_param.h"
#include "quote.h"
#include "html_feed_env.h"
#include "form.h"
#include "buffer.h"
#include "line_layout.h"
#include "http_response.h"
#include "Str.h"
#include "myctype.h"
#include "ctrlcode.h"
#include "html_tag.h"
#include "proc.h"
#include "stringtoken.h"
#include "html_parser.h"
#include <math.h>
#include <string_view>
#include <sstream>

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
  this->prevchar = " ";
  this->flag = RB_IGNORE_P;
  this->status = R_ST_NORMAL;
  this->prev_ctype = PC_ASCII;
  this->bp.init_flag = 1;
  this->set_breakpoint(0);
}

void readbuffer::set_alignment(const std::shared_ptr<HtmlTag> &tag) {
  auto flag = (ReadBufferFlags)-1;
  if (auto value = tag->parsedtag_get_value(ATTR_ALIGN)) {
    switch (stoi(*value)) {
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
  this->RB_SAVE_FLAG();
  if (flag != (ReadBufferFlags)-1) {
    this->RB_SET_ALIGN(flag);
  }
}

/*
 * Check character type
 */
#define LINELEN 256 /* Initial line length */

static Str *checkType(Str *s, std::vector<Lineprop> *oprop) {
  static std::vector<Lineprop> prop_buffer;

  char *str = s->ptr;
  char *endp = &s->ptr[s->length];
  char *bs = NULL;

  if (prop_buffer.size() < s->length) {
    prop_buffer.resize(s->length > LINELEN ? s->length : LINELEN);
  }
  auto prop = prop_buffer.data();

  bool do_copy = false;
  if (!do_copy) {
    for (; str < endp && IS_ASCII(*str); str++) {
      *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
    }
  }

  Lineprop effect = PE_NORMAL;
  while (str < endp) {
    if (prop - prop_buffer.data() >= prop_buffer.size())
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

void loadBuffer(const std::shared_ptr<LineLayout> &layout, int width,
                HttpResponse *res, std::string_view page) {
  layout->clear();
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

    std::vector<Lineprop> propBuffer;
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
std::shared_ptr<HttpResponse> getshell(const std::string &cmd) {
#ifdef _MSC_VER
  return {};
#else
  if (cmd.empty()) {
    return {};
  }

  auto f = popen(cmd, "r");
  if (f == nullptr) {
    return {};
  }

  // auto buf = Buffer::create();
  // UrlStream uf(SCM_UNKNOWN, newFileStream(f, pclose));
  // TODO:
  // loadBuffer(buf->res.get(), &buf->layout);
  // buf->res->filename = cmd;
  // buf->layout->data.title = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  // if (res->type.empty()) {
  //   buf->res->type = "text/plain";
  // }
  return {};
#endif
}

void readbuffer::set_breakpoint(int tag_length) {
  this->bp.len = this->line.size();
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

void readbuffer::push_link(HtmlCommand cmd, int offset, int pos) {
  link_stack.push_front({
      .cmd = cmd,
      .offset = (short)offset,
      .pos = (short)pos,
  });
  auto p = &link_stack.front();
  if (p->offset < 0)
    p->offset = 0;
  if (p->pos < 0)
    p->pos = 0;
}

void readbuffer::append_tags() {
  int len = this->line.size();
  bool set_bp = false;
  for (int i = 0; i < this->tag_sp; i++) {
    switch (this->tag_stack[i]->cmd) {
    case HTML_A:
    case HTML_IMG_ALT:
    case HTML_B:
    case HTML_U:
    case HTML_I:
    case HTML_S:
      this->push_link(this->tag_stack[i]->cmd, this->line.size(), this->pos);
      break;
    }
    this->line += this->tag_stack[i]->cmdname;
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
    this->set_breakpoint(this->line.size() - len);
  }
}

const char *readbuffer::has_hidden_link(HtmlCommand cmd) const {
  // Str *line = this->line;
  if (line.back() != '>')
    return nullptr;

  auto p = link_stack.begin();
  for (; p != link_stack.end(); ++p)
    if (p->cmd == cmd)
      break;
  if (p == link_stack.end())
    return nullptr;

  if (this->pos == p->pos)
    return line.c_str() + p->offset;

  return nullptr;
}

void readbuffer::passthrough(const char *str, int back) {
  HtmlCommand cmd;
  Str *tok = Strnew();

  if (back) {
    Str *str_save = Strnew_charp(str);
    Strshrink(this->line, this->line.c_str() + this->line.size() - str);
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
        for (auto p = link_stack.begin(); p != link_stack.end(); ++p) {
          if (p->cmd == cmd) {
            for (auto i = link_stack.begin(); i != p;) {
              i = link_stack.erase(i);
            }
            // link_stack = p->next;
            break;
          }
        }
        back = 0;
      } else {
        Strcat(tok, *token);
        this->push_tag(tok->ptr, cmd);
        Strclear(tok);
      }
    } else {
      this->push_nchars(0, str_bak, str - str_bak, this->prev_ctype);
    }
  }
}

void readbuffer::push_tag(const std::string &cmdname, HtmlCommand cmd) {
  this->tag_stack[this->tag_sp] = std::make_shared<cmdtable>();
  this->tag_stack[this->tag_sp]->cmdname = cmdname;
  this->tag_stack[this->tag_sp]->cmd = cmd;
  this->tag_sp++;
  if (this->tag_sp >= TAG_STACK_SIZE || this->flag & (RB_SPECIAL & ~RB_NOBR)) {
    this->append_tags();
  }
}

void readbuffer::push_nchars(int width, const char *str, int len,
                             Lineprop mode) {
  this->append_tags();
  this->line += std::string(str, len);
  this->pos += width;
  if (width > 0) {
    this->prevchar = std::string(str, len);
    this->prev_ctype = mode;
  }
  this->flag |= RB_NFLUSHED;
}

void readbuffer::proc_mchar(int pre_mode, int width, const char **str,
                            Lineprop mode) {
  this->check_breakpoint(pre_mode, *str);
  this->pos += width;
  this->line += std::string(*str, get_mclen(*str));
  if (width > 0) {
    this->prevchar = std::string(*str, 1);
    if (**str != ' ')
      this->prev_ctype = mode;
  }
  (*str) += get_mclen(*str);
  this->flag |= RB_NFLUSHED;
}

void readbuffer::flushline(const std::shared_ptr<GeneralList> &buf, int indent,
                           int force, int width) {
  // Str *line = this->line;
  Str *pass = nullptr;
  // char *hidden_anchor = nullptr, *hidden_img = nullptr, *hidden_bold =
  // nullptr,
  //      *hidden_under = nullptr, *hidden_italic = nullptr, *hidden_strike =
  //      nullptr, *hidden_ins = nullptr, *hidden_input = nullptr, *hidden =
  //      nullptr;
  const char *hidden = nullptr;
  const char *hidden_anchor = nullptr;

  if (!(this->flag & (RB_SPECIAL & ~RB_NOBR)) && line.size() &&
      line.back() == ' ') {
    line.pop_back();
    this->pos--;
  }

  this->append_tags();

  if (this->anchor.url.size()) {
    hidden = hidden_anchor = this->has_hidden_link(HTML_A);
  }

  const char *hidden_img = nullptr;
  if (this->img_alt.size()) {
    if ((hidden_img = this->has_hidden_link(HTML_IMG_ALT))) {
      if (!hidden || hidden_img < hidden)
        hidden = hidden_img;
    }
  }

  const char *hidden_input = nullptr;
  if (this->input_alt.in) {
    if ((hidden_input = this->has_hidden_link(HTML_INPUT_ALT))) {
      if (!hidden || hidden_input < hidden) {
        hidden = hidden_input;
      }
    }
  }

  const char *hidden_bold = nullptr;
  if (this->fontstat.in_bold) {
    if ((hidden_bold = this->has_hidden_link(HTML_B))) {
      if (!hidden || hidden_bold < hidden) {
        hidden = hidden_bold;
      }
    }
  }

  const char *hidden_italic = nullptr;
  if (this->fontstat.in_italic) {
    if ((hidden_italic = this->has_hidden_link(HTML_I))) {
      if (!hidden || hidden_italic < hidden) {
        hidden = hidden_italic;
      }
    }
  }

  const char *hidden_under = nullptr;
  if (this->fontstat.in_under) {
    if ((hidden_under = this->has_hidden_link(HTML_U))) {
      if (!hidden || hidden_under < hidden) {
        hidden = hidden_under;
      }
    }
  }

  const char *hidden_strike = nullptr;
  if (this->fontstat.in_strike) {
    if ((hidden_strike = this->has_hidden_link(HTML_S))) {
      if (!hidden || hidden_strike < hidden) {
        hidden = hidden_strike;
      }
    }
  }

  const char *hidden_ins = nullptr;
  if (this->fontstat.in_ins) {
    if ((hidden_ins = this->has_hidden_link(HTML_INS))) {
      if (!hidden || hidden_ins < hidden) {
        hidden = hidden_ins;
      }
    }
  }

  if (hidden) {
    pass = Strnew_charp(hidden);
    Strshrink(line, line.c_str() + line.size() - hidden);
  }

  if (!(this->flag & (RB_SPECIAL & ~RB_NOBR)) && this->pos > width) {
    char *tp = &line[this->bp.len - this->bp.tlen];
    char *ep = &line[line.size()];

    if (this->bp.pos == this->pos && tp <= ep && tp > line.c_str() &&
        tp[-1] == ' ') {
      memcpy(tp - 1, tp, ep - tp + 1);
      line.pop_back();
      this->pos--;
    }
  }

  if (this->anchor.url.size() && !hidden_anchor)
    line += "</a>";
  if (this->img_alt.size() && !hidden_img)
    line += "</img_alt>";
  if (this->input_alt.in && !hidden_input)
    line += "</input_alt>";
  if (this->fontstat.in_bold && !hidden_bold)
    line += "</b>";
  if (this->fontstat.in_italic && !hidden_italic)
    line += "</i>";
  if (this->fontstat.in_under && !hidden_under)
    line += "</u>";
  if (this->fontstat.in_strike && !hidden_strike)
    line += "</s>";
  if (this->fontstat.in_ins && !hidden_ins)
    line += "</ins>";

  if (this->top_margin > 0) {
    int i;

    struct html_feed_environ h(1, width, indent);
    h.obuf.line = {};
    h.obuf.pos = this->pos;
    h.obuf.flag = this->flag;
    h.obuf.top_margin = -1;
    h.obuf.bottom_margin = -1;
    h.obuf.line += "<pre_int>";
    for (i = 0; i < h.obuf.pos; i++)
      h.obuf.line += ' ';
    h.obuf.line += "</pre_int>";
    for (i = 0; i < this->top_margin; i++) {
      flushline(buf, indent, force, width);
    }
  }

  if (force == 1 || this->flag & RB_NFLUSHED) {
    auto lbuf = std::make_shared<TextLine>(line.c_str(), this->pos);
    if (this->RB_GET_ALIGN() == RB_CENTER) {
      lbuf->align(width, ALIGN_CENTER);
    } else if (this->RB_GET_ALIGN() == RB_RIGHT) {
      lbuf->align(width, ALIGN_RIGHT);
    } else if (this->RB_GET_ALIGN() == RB_LEFT && this->flag & RB_INTABLE) {
      lbuf->align(width, ALIGN_LEFT);
    } else if (this->flag & RB_FILL) {
      const char *p;
      int rest, rrest;
      int nspace, d, i;

      rest = width - get_strwidth(line.c_str());
      if (rest > 1) {
        nspace = 0;
        for (p = line.c_str() + indent; *p; p++) {
          if (*p == ' ')
            nspace++;
        }
        if (nspace > 0) {
          int indent_here = 0;
          d = rest / nspace;
          p = line.c_str();
          while (IS_SPACE(*p)) {
            p++;
            indent_here++;
          }
          rrest = rest - d * nspace;
          line = {};
          for (i = 0; i < indent_here; i++)
            line += ' ';
          for (; *p; p++) {
            line += *p;
            if (*p == ' ') {
              for (i = 0; i < d; i++)
                line += ' ';
              if (rrest > 0) {
                line += ' ';
                rrest--;
              }
            }
          }
          lbuf = std::make_shared<TextLine>(line.c_str(), width);
        }
      }
    }

    if (lbuf->pos > this->maxlimit) {
      this->maxlimit = lbuf->pos;
    }

    if (buf) {
      buf->_list.push_back(lbuf);
    }
    if (this->flag & RB_SPECIAL || this->flag & RB_NFLUSHED)
      this->blank_lines = 0;
    else
      this->blank_lines++;
  } else {
    const char *p = line.c_str();
    std::stringstream tmp;
    std::stringstream tmp2;

    while (*p) {
      stringtoken st(p);
      auto token = st.sloppy_parse_line();
      p = st.ptr();
      if (token) {
        tmp << *token;
        if (force == 2) {
          if (buf) {
            buf->appendTextLine(tmp.str(), 0);
          }
        } else
          tmp2 << tmp.str();
        tmp.str("");
      }
    }
    if (force == 2) {
      if (pass) {
        if (buf) {
          buf->appendTextLine(pass->ptr, 0);
        }
      }
      pass = nullptr;
    } else {
      if (pass)
        tmp2 << pass->ptr;
      pass = Strnew(tmp2.str());
    }
  }

  if (this->bottom_margin > 0) {
    int i;

    struct html_feed_environ h(1, width, indent);
    h.obuf.line = {};
    h.obuf.pos = this->pos;
    h.obuf.flag = this->flag;
    h.obuf.top_margin = -1;
    h.obuf.bottom_margin = -1;
    h.obuf.line += "<pre_int>";
    for (i = 0; i < h.obuf.pos; i++)
      h.obuf.line += ' ';
    h.obuf.line += "</pre_int>";
    for (i = 0; i < this->bottom_margin; i++) {
      flushline(buf, indent, force, width);
    }
  }
  if (this->top_margin < 0 || this->bottom_margin < 0) {
    return;
  }

  this->line = {};
  this->pos = 0;
  this->top_margin = 0;
  this->bottom_margin = 0;
  this->prevchar = " ";
  this->bp.init_flag = 1;
  this->flag &= ~RB_NFLUSHED;
  this->set_breakpoint(0);
  this->prev_ctype = PC_ASCII;
  this->link_stack.clear();
  this->fillline(indent);
  if (pass)
    this->passthrough(pass->ptr, 0);
  if (!hidden_anchor && this->anchor.url.size()) {
    Str *tmp;
    if (this->anchor.hseq > 0)
      this->anchor.hseq = -this->anchor.hseq;
    tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", this->anchor.hseq);
    Strcat(tmp, html_quote(this->anchor.url.c_str()));
    if (this->anchor.target.size()) {
      Strcat_charp(tmp, "\" TARGET=\"");
      Strcat(tmp, html_quote(this->anchor.target.c_str()));
    }
    if (this->anchor.option.use_referer()) {
      Strcat_charp(tmp, "\" REFERER=\"");
      Strcat(tmp, html_quote(this->anchor.option.referer.c_str()));
    }
    if (this->anchor.title.size()) {
      Strcat_charp(tmp, "\" TITLE=\"");
      Strcat(tmp, html_quote(this->anchor.title.c_str()));
    }
    if (this->anchor.accesskey) {
      auto c = html_quote_char(this->anchor.accesskey);
      Strcat_charp(tmp, "\" ACCESSKEY=\"");
      if (c)
        Strcat_charp(tmp, c);
      else
        Strcat_char(tmp, this->anchor.accesskey);
    }
    Strcat_charp(tmp, "\">");
    this->push_tag(tmp->ptr, HTML_A);
  }
  if (!hidden_img && this->img_alt.size()) {
    Str *tmp = Strnew_charp("<IMG_ALT SRC=\"");
    Strcat(tmp, html_quote(this->img_alt));
    Strcat_charp(tmp, "\">");
    this->push_tag(tmp->ptr, HTML_IMG_ALT);
  }
  if (!hidden_input && this->input_alt.in) {
    if (this->input_alt.hseq > 0)
      this->input_alt.hseq = -this->input_alt.hseq;
    std::stringstream tmp;
    tmp << "<INPUT_ALT hseq=\"" << this->input_alt.hseq << "\" fid=\""
        << this->input_alt.fid << "\" name=\"" << this->input_alt.name
        << "\" type=\"" << this->input_alt.type << "\" "
        << "value=\"" << this->input_alt.value << "\">";
    this->push_tag(tmp.str(), HTML_INPUT_ALT);
  }
  if (!hidden_bold && this->fontstat.in_bold)
    this->push_tag("<B>", HTML_B);
  if (!hidden_italic && this->fontstat.in_italic)
    this->push_tag("<I>", HTML_I);
  if (!hidden_under && this->fontstat.in_under)
    this->push_tag("<U>", HTML_U);
  if (!hidden_strike && this->fontstat.in_strike)
    this->push_tag("<S>", HTML_S);
  if (!hidden_ins && this->fontstat.in_ins)
    this->push_tag("<INS>", HTML_INS);
}

void readbuffer::CLOSE_P(struct html_feed_environ *h_env) {
  if (this->flag & RB_P) {
    struct environment *envs = h_env->envs.data();
    this->flushline(h_env->buf, envs[h_env->envc].indent, 0, h_env->limit);
    this->RB_RESTORE_FLAG();
    this->flag &= ~RB_P;
  }
}
