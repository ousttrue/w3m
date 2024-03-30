#include "html_feed_env.h"
#include "push_symbol.h"
#include "option_param.h"
#include "form.h"
#include "readbuffer.h"
#include "cmp.h"
#include "stringtoken.h"
#include "html_quote.h"
#include "ctrlcode.h"
#include "symbol.h"
#include "utf8.h"
#include <assert.h>
#include <sstream>

html_feed_environ::html_feed_environ(int nenv, int limit, int indent,
                                     const std::shared_ptr<GeneralList> &_buf)
    : buf(_buf), parser(limit) {
  assert(nenv);
  envs.resize(nenv);
  this->tagbuf = Strnew();
  this->limit = limit;
  envs[0].indent = indent;
}

void html_feed_environ::purgeline() {
  if (this->buf == NULL || this->obuf.blank_lines == 0)
    return;

  std::shared_ptr<TextLine> tl;
  if (!(tl = this->buf->rpopValue()))
    return;

  const char *p = tl->line.c_str();
  std::stringstream tmp;
  while (*p) {
    stringtoken st(p);
    auto token = st.sloppy_parse_line();
    p = st.ptr();
    if (token) {
      tmp << *token;
    }
  }
  this->buf->appendTextLine(tmp.str(), 0);
  this->obuf.blank_lines--;
}

void html_feed_environ::POP_ENV() {
  if (this->envc_real-- < (int)this->envs.size()) {
    this->envc--;
  }
}

void html_feed_environ::PUSH_ENV_NOINDENT(HtmlCommand cmd) {
  if (++this->envc_real < (int)this->envs.size()) {
    ++this->envc;
    envs[this->envc].env = cmd;
    envs[this->envc].count = 0;
    envs[this->envc].indent = envs[this->envc - 1].indent;
  }
}

void html_feed_environ::PUSH_ENV(HtmlCommand cmd) {
  if (++this->envc_real < (int)this->envs.size()) {
    ++this->envc;
    envs[this->envc].env = cmd;
    envs[this->envc].count = 0;
    if (this->envc <= MAX_INDENT_LEVEL)
      envs[this->envc].indent = envs[this->envc - 1].indent + INDENT_INCR;
    else
      envs[this->envc].indent = envs[this->envc - 1].indent;
  }
}

void html_feed_environ::parseLine(std::string_view istr, bool internal) {
  parser.parse(istr, this, internal);
}

void html_feed_environ::completeHTMLstream() {
  parser.completeHTMLstream(this);
}

std::shared_ptr<LineLayout>
html_feed_environ::render(const std::shared_ptr<LineLayout> &layout,
                          const Url &currentUrl) {
  return parser.render(layout, currentUrl, this);
}

int html_feed_environ::HTML_Paragraph(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 1, this->limit);
    this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  this->obuf.flag |= RB_IGNORE_P;
  if (tag->tagid == HTML_P) {
    this->obuf.set_alignment(tag);
    this->obuf.flag |= RB_P;
  }
  return 1;
}

int html_feed_environ::HTML_H_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (!(this->obuf.flag & (RB_PREMODE | RB_IGNORE_P))) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  this->parser.HTMLlineproc1("<b>", this);
  this->obuf.set_alignment(tag);
  return 1;
}

int html_feed_environ::HTML_H_exit() {
  this->parser.HTMLlineproc1("</b>", this);
  if (!(this->obuf.flag & RB_PREMODE)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.RB_RESTORE_FLAG();
  this->parser.close_anchor(this);
  this->obuf.flag |= RB_IGNORE_P;
  return 1;
}

int html_feed_environ::HTML_List_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    if (!(this->obuf.flag & RB_PREMODE) &&
        (this->envc == 0 || tag->tagid == HTML_BLQ))
      this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0,
                              this->limit);
  }
  this->PUSH_ENV(tag->tagid);
  if (tag->tagid == HTML_UL || tag->tagid == HTML_OL) {
    if (auto value = tag->parsedtag_get_value(ATTR_START)) {
      envs[this->envc].count = stoi(*value) - 1;
    }
  }
  if (tag->tagid == HTML_OL) {
    envs[this->envc].type = '1';
    if (auto value = tag->parsedtag_get_value(ATTR_TYPE)) {
      envs[this->envc].type = (*value)[0];
    }
  }
  if (tag->tagid == HTML_UL) {
    envs[this->envc].type = tag->ul_type(0);
  }
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  return 1;
}

int html_feed_environ::HTML_List_exit(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_DT(&this->obuf, this);
  this->parser.CLOSE_A(&this->obuf, this);
  if (this->envc > 0) {
    this->obuf.flushline(this->buf, envs[this->envc - 1].indent, 0,
                         this->limit);
    this->POP_ENV();
    if (!(this->obuf.flag & RB_PREMODE) &&
        (this->envc == 0 || tag->tagid == HTML_N_BLQ)) {
      this->obuf.do_blankline(this->buf, envs[this->envc].indent, INDENT_INCR,
                              this->limit);
      this->obuf.flag |= RB_IGNORE_P;
    }
  }
  this->parser.close_anchor(this);
  return 1;
}

int html_feed_environ::HTML_DL_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    if (!(this->obuf.flag & RB_PREMODE) && envs[this->envc].env != HTML_DL &&
        envs[this->envc].env != HTML_DL_COMPACT &&
        envs[this->envc].env != HTML_DD)
      this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0,
                              this->limit);
  }
  this->PUSH_ENV_NOINDENT(tag->tagid);
  if (tag->parsedtag_exists(ATTR_COMPACT))
    envs[this->envc].env = HTML_DL_COMPACT;
  this->obuf.flag |= RB_IGNORE_P;
  return 1;
}

int html_feed_environ::HTML_LI_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_A(&this->obuf, this);
  this->parser.CLOSE_DT(&this->obuf, this);
  if (this->envc > 0) {
    Str *num;
    this->obuf.flushline(this->buf, envs[this->envc - 1].indent, 0,
                         this->limit);
    envs[this->envc].count++;
    if (auto value = tag->parsedtag_get_value(ATTR_VALUE)) {
      int count = stoi(*value);
      if (count > 0)
        envs[this->envc].count = count;
      else
        envs[this->envc].count = 0;
    }
    switch (envs[this->envc].env) {
    case HTML_UL: {
      envs[this->envc].type = tag->ul_type(envs[this->envc].type);
      for (int i = 0; i < INDENT_INCR - 3; i++)
        this->obuf.push_charp(1, NBSP, PC_ASCII);
      auto tmp = Strnew();
      switch (envs[this->envc].type) {
      case 'd':
        push_symbol(tmp, UL_SYMBOL_DISC, 1, 1);
        break;
      case 'c':
        push_symbol(tmp, UL_SYMBOL_CIRCLE, 1, 1);
        break;
      case 's':
        push_symbol(tmp, UL_SYMBOL_SQUARE, 1, 1);
        break;
      default:
        push_symbol(tmp, UL_SYMBOL((this->envc_real - 1) % MAX_UL_LEVEL), 1, 1);
        break;
      }
      this->obuf.push_charp(1, NBSP, PC_ASCII);
      this->obuf.push_str(1, tmp->ptr, PC_ASCII);
      this->obuf.push_charp(1, NBSP, PC_ASCII);
      this->obuf.prevchar = " ";
      break;
    }

    case HTML_OL:
      if (auto value = tag->parsedtag_get_value(ATTR_TYPE))
        envs[this->envc].type = (*value)[0];
      switch ((envs[this->envc].count > 0) ? envs[this->envc].type : '1') {
      case 'i':
        num = romanNumeral(envs[this->envc].count);
        break;
      case 'I':
        num = romanNumeral(envs[this->envc].count);
        Strupper(num);
        break;
      case 'a':
        num = romanAlphabet(envs[this->envc].count);
        break;
      case 'A':
        num = romanAlphabet(envs[this->envc].count);
        Strupper(num);
        break;
      default:
        num = Sprintf("%d", envs[this->envc].count);
        break;
      }
      if (INDENT_INCR >= 4)
        Strcat_charp(num, ". ");
      else
        Strcat_char(num, '.');
      this->obuf.push_spaces(1, INDENT_INCR - num->length);
      this->obuf.push_str(num->length, num->ptr, PC_ASCII);
      if (INDENT_INCR >= 4)
        this->obuf.prevchar = " ";
      break;
    default:
      this->obuf.push_spaces(1, INDENT_INCR);
      break;
    }
  } else {
    this->obuf.flushline(this->buf, 0, 0, this->limit);
  }
  this->obuf.flag |= RB_IGNORE_P;
  return 1;
}

int html_feed_environ::HTML_DT_enter() {
  this->parser.CLOSE_A(&this->obuf, this);
  if (this->envc == 0 || (this->envc_real < (int)this->envs.size() &&
                          envs[this->envc].env != HTML_DL &&
                          envs[this->envc].env != HTML_DL_COMPACT)) {
    this->PUSH_ENV_NOINDENT(HTML_DL);
  }
  if (this->envc > 0) {
    this->obuf.flushline(this->buf, envs[this->envc - 1].indent, 0,
                         this->limit);
  }
  if (!(this->obuf.flag & RB_IN_DT)) {
    this->parser.HTMLlineproc1("<b>", this);
    this->obuf.flag |= RB_IN_DT;
  }
  this->obuf.flag |= RB_IGNORE_P;
  return 1;
}

int html_feed_environ::HTML_DT_exit() {
  if (!(this->obuf.flag & RB_IN_DT)) {
    return 1;
  }
  this->obuf.flag &= ~RB_IN_DT;
  this->parser.HTMLlineproc1("</b>", this);
  if (this->envc > 0 && envs[this->envc].env == HTML_DL)
    this->obuf.flushline(this->buf, envs[this->envc - 1].indent, 0,
                         this->limit);
  return 1;
}

int html_feed_environ::HTML_DD_enter() {
  this->parser.CLOSE_A(&this->obuf, this);
  this->parser.CLOSE_DT(&this->obuf, this);
  if (envs[this->envc].env == HTML_DL ||
      envs[this->envc].env == HTML_DL_COMPACT) {
    this->PUSH_ENV(HTML_DD);
  }

  if (this->envc > 0 && envs[this->envc - 1].env == HTML_DL_COMPACT) {
    if (this->obuf.pos > envs[this->envc].indent)
      this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    else
      this->obuf.push_spaces(1, envs[this->envc].indent - this->obuf.pos);
  } else
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  /* this->obuf.flag |= RB_IGNORE_P; */
  return 1;
}

int html_feed_environ::HTML_TITLE_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.close_anchor(this);
  this->parser.process_title(tag);
  this->obuf.flag |= RB_TITLE;
  this->obuf.end_tag = HTML_N_TITLE;
  return 1;
}

int html_feed_environ::HTML_TITLE_exit(const std::shared_ptr<HtmlTag> &tag) {
  if (!(this->obuf.flag & RB_TITLE))
    return 1;
  this->obuf.flag &= ~RB_TITLE;
  this->obuf.end_tag = {};
  auto tmp = this->parser.process_n_title(tag);
  if (tmp.size())
    this->parser.HTMLlineproc1(tmp.c_str(), this);
  return 1;
}

int html_feed_environ::HTML_TITLE_ALT(const std::shared_ptr<HtmlTag> &tag) {
  if (auto value = tag->parsedtag_get_value(ATTR_TITLE))
    this->title = html_unquote(*value);
  return 0;
}

int html_feed_environ::HTML_FRAMESET_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->PUSH_ENV(tag->tagid);
  this->obuf.push_charp(9, "--FRAME--", PC_ASCII);
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  return 0;
}

int html_feed_environ::HTML_FRAMESET_exit() {
  if (this->envc > 0) {
    this->POP_ENV();
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  return 0;
}

int html_feed_environ::HTML_NOFRAMES_enter() {
  this->parser.CLOSE_A(&this->obuf, this);
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.flag |= (RB_NOFRAMES | RB_IGNORE_P);
  /* istr = str; */
  return 1;
}

int html_feed_environ::HTML_NOFRAMES_exit() {
  this->parser.CLOSE_A(&this->obuf, this);
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.flag &= ~RB_NOFRAMES;
  return 1;
}

int html_feed_environ::HTML_FRAME(const std::shared_ptr<HtmlTag> &tag) {
  std::string q;
  if (auto value = tag->parsedtag_get_value(ATTR_SRC))
    q = *value;
  std::string r;
  if (auto value = tag->parsedtag_get_value(ATTR_NAME))
    r = *value;
  if (q.size()) {
    q = html_quote(q);
    this->obuf.push_tag(Sprintf("<a hseq=\"%d\" href=\"%s\">",
                                this->parser.cur_hseq++, q.c_str())
                            ->ptr,
                        HTML_A);
    if (r.size())
      q = html_quote(r);
    this->obuf.push_charp(get_strwidth(q), q.c_str(), PC_ASCII);
    this->obuf.push_tag("</a>", HTML_N_A);
  }
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  return 0;
}

int html_feed_environ::HTML_HR(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.close_anchor(this);
  auto tmp = this->parser.process_hr(tag, this->limit, envs[this->envc].indent);
  this->parser.HTMLlineproc1(tmp, this);
  this->obuf.prevchar = " ";
  return 1;
}

int html_feed_environ::HTML_PRE_enter(const std::shared_ptr<HtmlTag> &tag) {
  int x = tag->parsedtag_exists(ATTR_FOR_TABLE);
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    if (!x)
      this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0,
                              this->limit);
  } else
    this->obuf.fillline(envs[this->envc].indent);
  this->obuf.flag |= (RB_PRE | RB_IGNORE_P);
  /* istr = str; */
  return 1;
}

int html_feed_environ::HTML_PRE_exit() {
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.flag |= RB_IGNORE_P;
    this->obuf.blank_lines++;
  }
  this->obuf.flag &= ~RB_PRE;
  this->parser.close_anchor(this);
  return 1;
}

int html_feed_environ::HTML_PRE_PLAIN_enter() {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  this->obuf.flag |= (RB_PRE | RB_IGNORE_P);
  return 1;
}

int html_feed_environ::HTML_PRE_PLAIN_exit() {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.flag |= RB_IGNORE_P;
  }
  this->obuf.flag &= ~RB_PRE;
  return 1;
}

int html_feed_environ::HTML_PLAINTEXT_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  this->obuf.flag |= (RB_PLAIN | RB_IGNORE_P);
  switch (tag->tagid) {
  case HTML_LISTING:
    this->obuf.end_tag = HTML_N_LISTING;
    break;
  case HTML_XMP:
    this->obuf.end_tag = HTML_N_XMP;
    break;
  case HTML_PLAINTEXT:
    this->obuf.end_tag = MAX_HTMLTAG;
    break;

  default:
    break;
  }
  return 1;
}

int html_feed_environ::HTML_LISTING_exit() {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0, this->limit);
    this->obuf.flag |= RB_IGNORE_P;
  }
  this->obuf.flag &= ~RB_PLAIN;
  this->obuf.end_tag = {};
  return 1;
}

int html_feed_environ::HTML_A_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (this->obuf.anchor.url.size()) {
    this->parser.close_anchor(this);
  }

  if (auto value = tag->parsedtag_get_value(ATTR_HREF))
    this->obuf.anchor.url = *value;
  if (auto value = tag->parsedtag_get_value(ATTR_TARGET))
    this->obuf.anchor.target = *value;
  if (auto value = tag->parsedtag_get_value(ATTR_REFERER))
    this->obuf.anchor.option = {.referer = *value};
  if (auto value = tag->parsedtag_get_value(ATTR_TITLE))
    this->obuf.anchor.title = *value;
  if (auto value = tag->parsedtag_get_value(ATTR_ACCESSKEY))
    this->obuf.anchor.accesskey = (*value)[0];
  if (auto value = tag->parsedtag_get_value(ATTR_HSEQ))
    this->obuf.anchor.hseq = stoi(*value);

  if (this->obuf.anchor.hseq == 0 && this->obuf.anchor.url.size()) {
    this->obuf.anchor.hseq = this->parser.cur_hseq;
    auto tmp = this->parser.process_anchor(tag, this->tagbuf->ptr);
    this->obuf.push_tag(tmp.c_str(), HTML_A);
    return 1;
  }
  return 0;
}

int html_feed_environ::HTML_IMG_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (tag->parsedtag_exists(ATTR_USEMAP))
    this->parser.HTML5_CLOSE_A(&this->obuf, this);
  auto tmp = this->parser.process_img(tag, this->limit);
  this->parser.HTMLlineproc1(tmp, this);
  return 1;
}

int html_feed_environ::HTML_IMG_ALT_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (auto value = tag->parsedtag_get_value(ATTR_SRC))
    this->obuf.img_alt = *value;
  return 0;
}

int html_feed_environ::HTML_IMG_ALT_exit() {
  if (this->obuf.img_alt.size()) {
    if (!this->obuf.close_effect0(HTML_IMG_ALT))
      this->obuf.push_tag("</img_alt>", HTML_N_IMG_ALT);
    this->obuf.img_alt = {};
  }
  return 1;
}

int html_feed_environ::HTML_TABLE_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.close_anchor(this);
  if (this->obuf.table_level + 1 >= MAX_TABLE) {
    return -1;
  }
  this->obuf.table_level++;
  int w = BORDER_NONE;
  /* x: cellspacing, y: cellpadding */
  int x = 2;
  int y = 1;
  int z = 0;
  int width = 0;
  if (tag->parsedtag_exists(ATTR_BORDER)) {
    if (tag->parsedtag_get_value(ATTR_BORDER, &w)) {
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
  int i;
  if (tag->parsedtag_get_value(ATTR_WIDTH, &i)) {
    if (this->obuf.table_level == 0)
      width = REAL_WIDTH(i, this->limit - envs[this->envc].indent);
    else
      width = RELATIVE_WIDTH(i);
  }
  if (tag->parsedtag_exists(ATTR_HBORDER))
    w = BORDER_NOWIN;
  tag->parsedtag_get_value(ATTR_CELLSPACING, &x);
  tag->parsedtag_get_value(ATTR_CELLPADDING, &y);
  tag->parsedtag_get_value(ATTR_VSPACE, &z);
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
  this->parser.tables[this->obuf.table_level] = table::begin_table(w, x, y, z);
  this->parser.table_mode[this->obuf.table_level].pre_mode = {};
  this->parser.table_mode[this->obuf.table_level].indent_level = 0;
  this->parser.table_mode[this->obuf.table_level].nobr_level = 0;
  this->parser.table_mode[this->obuf.table_level].caption = 0;
  this->parser.table_mode[this->obuf.table_level].end_tag = HTML_UNKNOWN;
  this->parser.tables[this->obuf.table_level]->total_width = width;
  return 1;
}

int html_feed_environ::HTML_CENTER_enter() {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & (RB_PREMODE | RB_IGNORE_P)))
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.RB_SAVE_FLAG();
  if (DisableCenter) {
    this->obuf.RB_SET_ALIGN(RB_LEFT);
  } else {
    this->obuf.RB_SET_ALIGN(RB_CENTER);
  }
  return 1;
}

int html_feed_environ::HTML_CENTER_exit() {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_PREMODE)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  this->obuf.RB_RESTORE_FLAG();
  return 1;
}

int html_feed_environ::HTML_DIV_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P)) {
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  }
  this->obuf.set_alignment(tag);
  return 1;
}

int html_feed_environ::HTML_DIV_exit() {
  this->parser.CLOSE_A(&this->obuf, this);
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.RB_RESTORE_FLAG();
  return 1;
}

int html_feed_environ::HTML_DIV_INT_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->obuf.CLOSE_P(this);
  if (!(this->obuf.flag & RB_IGNORE_P))
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.set_alignment(tag);
  return 1;
}

int html_feed_environ::HTML_DIV_INT_exit() {
  this->obuf.CLOSE_P(this);
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.RB_RESTORE_FLAG();
  return 1;
}

int html_feed_environ::HTML_FORM_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.CLOSE_A(&this->obuf, this);
  if (!(this->obuf.flag & RB_IGNORE_P))
    this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  auto tmp = this->parser.process_form(tag);
  if (tmp.size())
    this->parser.HTMLlineproc1(tmp, this);
  return 1;
}

int html_feed_environ::HTML_FORM_exit() {
  this->parser.CLOSE_A(&this->obuf, this);
  this->obuf.flushline(this->buf, envs[this->envc].indent, 0, this->limit);
  this->obuf.flag |= RB_IGNORE_P;
  this->parser.process_n_form();
  return 1;
}

int html_feed_environ::HTML_INPUT_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.close_anchor(this);
  auto tmp = this->parser.process_input(tag);
  if (tmp.size())
    this->parser.HTMLlineproc1(tmp, this);
  return 1;
}

int html_feed_environ::HTML_BUTTON_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.HTML5_CLOSE_A(&this->obuf, this);
  auto tmp = this->parser.process_button(tag);
  if (tmp.size())
    this->parser.HTMLlineproc1(tmp, this);
  return 1;
}

int html_feed_environ::HTML_BUTTON_exit() {
  auto tmp = this->parser.process_n_button();
  if (tmp.size())
    this->parser.HTMLlineproc1(tmp, this);
  return 1;
}

int html_feed_environ::HTML_SELECT_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.close_anchor(this);
  auto tmp = this->parser.process_select(tag);
  if (tmp.size())
    this->parser.HTMLlineproc1(tmp, this);
  this->obuf.flag |= RB_INSELECT;
  this->obuf.end_tag = HTML_N_SELECT;
  return 1;
}

int html_feed_environ::HTML_SELECT_exit() {
  this->obuf.flag &= ~RB_INSELECT;
  this->obuf.end_tag = HTML_UNKNOWN;
  auto tmp = this->parser.process_n_select();
  if (tmp.size())
    this->parser.HTMLlineproc1(tmp, this);
  return 1;
}

int html_feed_environ::HTML_TEXTAREA_enter(
    const std::shared_ptr<HtmlTag> &tag) {
  this->parser.close_anchor(this);
  auto tmp = this->parser.process_textarea(tag, this->limit);
  if (tmp.size()) {
    this->parser.HTMLlineproc1(tmp, this);
  }
  this->obuf.flag |= RB_INTXTA;
  this->obuf.end_tag = HTML_N_TEXTAREA;
  return 1;
}

int html_feed_environ::HTML_TEXTAREA_exit() {
  this->obuf.flag &= ~RB_INTXTA;
  this->obuf.end_tag = HTML_UNKNOWN;
  auto tmp = this->parser.process_n_textarea();
  if (tmp.size()) {
    this->parser.HTMLlineproc1(tmp, this);
  }
  return 1;
}

int html_feed_environ::HTML_ISINDEX_enter(const std::shared_ptr<HtmlTag> &tag) {
  auto p = "";
  auto q = "!CURRENT_URL!";
  tag->parsedtag_get_value(ATTR_PROMPT, &p);
  tag->parsedtag_get_value(ATTR_ACTION, &q);
  auto tmp = Strnew_m_charp(
      "<form method=get action=\"", html_quote(q), "\">", html_quote(p),
      "<input type=text name=\"\" accept></form>", nullptr);
  this->parser.HTMLlineproc1(tmp->ptr, this);
  return 1;
}

int html_feed_environ::HTML_META_enter(const std::shared_ptr<HtmlTag> &tag) {
  const char *p = nullptr;
  tag->parsedtag_get_value(ATTR_HTTP_EQUIV, &p);
  const char *q = nullptr;
  tag->parsedtag_get_value(ATTR_CONTENT, &q);
  if (p && q && !strcasecmp(p, "refresh")) {
    auto meta = getMetaRefreshParam(q);
    std::stringstream ss;
    if (meta.url.size()) {
      auto qq = html_quote(meta.url);
      ss << "Refresh (" << meta.interval << " sec) <a href=\"" << qq << "\">"
         << qq << "</a>";
    } else if (meta.interval > 0) {
      ss << "Refresh (" << meta.interval << " sec)";
    }
    auto tmp = ss.str();
    if (tmp.size()) {
      this->parser.HTMLlineproc1(tmp, this);
      this->obuf.do_blankline(this->buf, envs[this->envc].indent, 0,
                              this->limit);
    }
  }
  return 1;
}

int html_feed_environ::HTML_DEL_enter() {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->obuf.flag |= RB_DEL;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parser.HTMLlineproc1("<U>[DEL:</U>", this);
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->obuf.fontstat.in_strike < FONTSTAT_MAX)
      this->obuf.fontstat.in_strike++;
    if (this->obuf.fontstat.in_strike == 1) {
      this->obuf.push_tag("<s>", HTML_S);
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_DEL_exit() {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->obuf.flag &= ~RB_DEL;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parser.HTMLlineproc1("<U>:DEL]</U>", this);
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->obuf.fontstat.in_strike == 0)
      return 1;
    if (this->obuf.fontstat.in_strike == 1 && this->obuf.close_effect0(HTML_S))
      this->obuf.fontstat.in_strike = 0;
    if (this->obuf.fontstat.in_strike > 0) {
      this->obuf.fontstat.in_strike--;
      if (this->obuf.fontstat.in_strike == 0) {
        this->obuf.push_tag("</s>", HTML_N_S);
      }
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_S_enter() {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->obuf.flag |= RB_S;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parser.HTMLlineproc1("<U>[S:</U>", this);
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->obuf.fontstat.in_strike < FONTSTAT_MAX)
      this->obuf.fontstat.in_strike++;
    if (this->obuf.fontstat.in_strike == 1) {
      this->obuf.push_tag("<s>", HTML_S);
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_S_exit() {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    this->obuf.flag &= ~RB_S;
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parser.HTMLlineproc1("<U>:S]</U>", this);
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->obuf.fontstat.in_strike == 0)
      return 1;
    if (this->obuf.fontstat.in_strike == 1 && this->obuf.close_effect0(HTML_S))
      this->obuf.fontstat.in_strike = 0;
    if (this->obuf.fontstat.in_strike > 0) {
      this->obuf.fontstat.in_strike--;
      if (this->obuf.fontstat.in_strike == 0) {
        this->obuf.push_tag("</s>", HTML_N_S);
      }
    }
  }
  return 1;
}

int html_feed_environ::HTML_INS_enter() {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parser.HTMLlineproc1("<U>[INS:</U>", this);
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->obuf.fontstat.in_ins < FONTSTAT_MAX)
      this->obuf.fontstat.in_ins++;
    if (this->obuf.fontstat.in_ins == 1) {
      this->obuf.push_tag("<ins>", HTML_INS);
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_INS_exit() {
  switch (displayInsDel) {
  case DISPLAY_INS_DEL_SIMPLE:
    break;
  case DISPLAY_INS_DEL_NORMAL:
    this->parser.HTMLlineproc1("<U>:INS]</U>", this);
    break;
  case DISPLAY_INS_DEL_FONTIFY:
    if (this->obuf.fontstat.in_ins == 0)
      return 1;
    if (this->obuf.fontstat.in_ins == 1 && this->obuf.close_effect0(HTML_INS))
      this->obuf.fontstat.in_ins = 0;
    if (this->obuf.fontstat.in_ins > 0) {
      this->obuf.fontstat.in_ins--;
      if (this->obuf.fontstat.in_ins == 0) {
        this->obuf.push_tag("</ins>", HTML_N_INS);
      }
    }
    break;
  }
  return 1;
}

int html_feed_environ::HTML_BGSOUND_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (view_unseenobject) {
    const char *p;
    if (tag->parsedtag_get_value(ATTR_SRC, &p)) {
      auto q = html_quote(p);
      auto s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
      this->parser.HTMLlineproc1(s->ptr, this);
    }
  }
  return 1;
}

int html_feed_environ::HTML_EMBED_enter(const std::shared_ptr<HtmlTag> &tag) {
  this->parser.HTML5_CLOSE_A(&this->obuf, this);
  if (view_unseenobject) {
    const char *p;
    if (tag->parsedtag_get_value(ATTR_SRC, &p)) {
      auto q = html_quote(p);
      auto s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
      this->parser.HTMLlineproc1(s->ptr, this);
    }
  }
  return 1;
}

int html_feed_environ::HTML_APPLET_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (view_unseenobject) {
    const char *p;
    if (tag->parsedtag_get_value(ATTR_ARCHIVE, &p)) {
      auto q = html_quote(p);
      auto s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
      this->parser.HTMLlineproc1(s->ptr, this);
    }
  }
  return 1;
}

int html_feed_environ::HTML_BODY_enter(const std::shared_ptr<HtmlTag> &tag) {
  if (view_unseenobject) {
    const char *p;
    if (tag->parsedtag_get_value(ATTR_BACKGROUND, &p)) {
      auto q = html_quote(p);
      auto s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
      this->parser.HTMLlineproc1(s->ptr, this);
    }
  }
  return 1;
}

#define FORM_I_TEXT_DEFAULT_SIZE 40
std::shared_ptr<FormItem>
html_feed_environ::createFormItem(const std::shared_ptr<HtmlTag> &tag) {
  auto item = std::make_shared<FormItem>();
  item->type = FORM_UNKNOWN;
  item->size = -1;
  item->rows = 0;
  item->checked = item->init_checked = 0;
  item->accept = 0;
  item->value = item->init_value = "";
  item->readonly = 0;
  char *p;
  if (tag->parsedtag_get_value(ATTR_TYPE, &p)) {
    item->type = formtype(p);
    if (item->size < 0 &&
        (item->type == FORM_INPUT_TEXT || item->type == FORM_INPUT_FILE ||
         item->type == FORM_INPUT_PASSWORD))
      item->size = FORM_I_TEXT_DEFAULT_SIZE;
  }
  if (tag->parsedtag_get_value(ATTR_NAME, &p))
    item->name = p;
  if (tag->parsedtag_get_value(ATTR_VALUE, &p))
    item->value = item->init_value = p;
  item->checked = item->init_checked = tag->parsedtag_exists(ATTR_CHECKED);
  item->accept = tag->parsedtag_exists(ATTR_ACCEPT);
  tag->parsedtag_get_value(ATTR_SIZE, &item->size);
  tag->parsedtag_get_value(ATTR_MAXLENGTH, &item->maxlength);
  item->readonly = tag->parsedtag_exists(ATTR_READONLY);
  int i;
  if (tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &i) && i >= 0 &&
      i < (int)this->parser.textarea_str.size()) {
    item->value = item->init_value = this->parser.textarea_str[i];
  }
  if (tag->parsedtag_get_value(ATTR_ROWS, &p))
    item->rows = atoi(p);
  if (item->type == FORM_UNKNOWN) {
    /* type attribute is missing. Ignore the tag. */
    return NULL;
  }
  if (item->type == FORM_INPUT_FILE && item->value.size()) {
    /* security hole ! */
    assert(false);
    return NULL;
  }
  return item;
}
