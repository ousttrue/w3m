#pragma once
#include "html_parser.h"
#include "html_command.h"
#include "html_tag.h"
#include "html_quote.h"
#include "readbuffer.h"
#include "textline.h"
#include "generallist.h"
#include "ctrlcode.h"
#include "symbol.h"
#include "utf8.h"
#include <stdio.h>
#include <vector>

#define MAX_INDENT_LEVEL 10
#define MAX_UL_LEVEL 9
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000

struct Str;
struct TextLineList;
struct readbuffer;

struct environment {
  HtmlCommand env;
  int type;
  int count;
  char indent;
};

struct html_feed_environ {
  readbuffer obuf;
  GeneralList<TextLine> *buf;
  Str *tagbuf;
  int limit;
  int maxlimit = 0;

  std::vector<environment> envs;
  int envc = 0;

  int envc_real = 0;
  std::string title;
  int blank_lines = 0;

  HtmlParser parser;

  html_feed_environ(int nenv, int limit_width, int indent,
                    GeneralList<TextLine> *_buf = nullptr);
  void purgeline();

  void POP_ENV() {
    if (this->envc_real-- < (int)this->envs.size()) {
      this->envc--;
    }
  }

  void PUSH_ENV_NOINDENT(HtmlCommand cmd) {
    if (++this->envc_real < (int)this->envs.size()) {
      ++this->envc;
      envs[this->envc].env = cmd;
      envs[this->envc].count = 0;
      envs[this->envc].indent = envs[this->envc - 1].indent;
    }
  }

  void PUSH_ENV(HtmlCommand cmd) {
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

  void parseLine(std::string_view istr, bool internal) {
    parser.parseLine(istr, this, internal);
  }

  void completeHTMLstream() { parser.completeHTMLstream(this); }

  void flushline(int indent, int force, int width) {
    parser.flushline(this, indent, force, width);
  }

  std::shared_ptr<LineLayout> render(HttpResponse *res) {
    return parser.render(res, this);
  }

  int HTML_B_enter() {
    if (this->obuf.fontstat.in_bold < FONTSTAT_MAX)
      this->obuf.fontstat.in_bold++;
    if (this->obuf.fontstat.in_bold > 1)
      return 1;
    return 0;
  }

  int HTML_B_exit() {
    if (this->obuf.fontstat.in_bold == 1 &&
        this->parser.close_effect0(&this->obuf, HTML_B))
      this->obuf.fontstat.in_bold = 0;
    if (this->obuf.fontstat.in_bold > 0) {
      this->obuf.fontstat.in_bold--;
      if (this->obuf.fontstat.in_bold == 0)
        return 0;
    }
    return 1;
  }

  int HTML_I_enter() {
    if (this->obuf.fontstat.in_italic < FONTSTAT_MAX)
      this->obuf.fontstat.in_italic++;
    if (this->obuf.fontstat.in_italic > 1)
      return 1;
    return 0;
  }

  int HTML_I_exit() {
    if (this->obuf.fontstat.in_italic == 1 &&
        this->parser.close_effect0(&this->obuf, HTML_I))
      this->obuf.fontstat.in_italic = 0;
    if (this->obuf.fontstat.in_italic > 0) {
      this->obuf.fontstat.in_italic--;
      if (this->obuf.fontstat.in_italic == 0)
        return 0;
    }
    return 1;
  }

  int HTML_U_enter() {
    if (this->obuf.fontstat.in_under < FONTSTAT_MAX)
      this->obuf.fontstat.in_under++;
    if (this->obuf.fontstat.in_under > 1)
      return 1;
    return 0;
  }

  int HTML_U_exit() {
    if (this->obuf.fontstat.in_under == 1 &&
        this->parser.close_effect0(&this->obuf, HTML_U))
      this->obuf.fontstat.in_under = 0;
    if (this->obuf.fontstat.in_under > 0) {
      this->obuf.fontstat.in_under--;
      if (this->obuf.fontstat.in_under == 0)
        return 0;
    }
    return 1;
  }

  static void set_alignment(struct readbuffer *obuf, struct HtmlTag *tag) {
    auto flag = (ReadBufferFlags)-1;
    int align;

    if (tag->parsedtag_get_value(ATTR_ALIGN, &align)) {
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
    obuf->RB_SAVE_FLAG();
    if (flag != (ReadBufferFlags)-1) {
      obuf->RB_SET_ALIGN(flag);
    }
  }

  int HTML_Paragraph(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 1, this->limit);
      this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                this->limit);
    }
    this->obuf.flag |= RB_IGNORE_P;
    if (tag->tagid == HTML_P) {
      set_alignment(&this->obuf, tag);
      this->obuf.flag |= RB_P;
    }
    return 1;
  }

  int HTML_H_enter(HtmlTag *tag) {
    if (!(this->obuf.flag & (RB_PREMODE | RB_IGNORE_P))) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                this->limit);
    }
    this->parser.HTMLlineproc1("<b>", this);
    set_alignment(&this->obuf, tag);
    return 1;
  }

  int HTML_H_exit() {
    this->parser.HTMLlineproc1("</b>", this);
    if (!(this->obuf.flag & RB_PREMODE)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    }
    this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                              this->limit);
    this->obuf.RB_RESTORE_FLAG();
    this->parser.close_anchor(this);
    this->obuf.flag |= RB_IGNORE_P;
    return 1;
  }

  int HTML_List_enter(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      if (!(this->obuf.flag & RB_PREMODE) &&
          (this->envc == 0 || tag->tagid == HTML_BLQ))
        this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                  this->limit);
    }
    this->PUSH_ENV(tag->tagid);
    if (tag->tagid == HTML_UL || tag->tagid == HTML_OL) {
      int count;
      if (tag->parsedtag_get_value(ATTR_START, &count)) {
        envs[this->envc].count = count - 1;
      }
    }
    if (tag->tagid == HTML_OL) {
      envs[this->envc].type = '1';
      const char *p;
      if (tag->parsedtag_get_value(ATTR_TYPE, &p)) {
        envs[this->envc].type = (int)*p;
      }
    }
    if (tag->tagid == HTML_UL) {
      envs[this->envc].type = tag->ul_type(0);
    }
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    return 1;
  }

  int HTML_List_exit(HtmlTag *tag) {
    this->parser.CLOSE_DT(&this->obuf, this);
    this->parser.CLOSE_A(&this->obuf, this);
    if (this->envc > 0) {
      this->parser.flushline(this, envs[this->envc - 1].indent, 0, this->limit);
      this->POP_ENV();
      if (!(this->obuf.flag & RB_PREMODE) &&
          (this->envc == 0 || tag->tagid == HTML_N_BLQ)) {
        this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent,
                                  INDENT_INCR, this->limit);
        this->obuf.flag |= RB_IGNORE_P;
      }
    }
    this->parser.close_anchor(this);
    return 1;
  }

  int HTML_DL_enter(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      if (!(this->obuf.flag & RB_PREMODE) && envs[this->envc].env != HTML_DL &&
          envs[this->envc].env != HTML_DL_COMPACT &&
          envs[this->envc].env != HTML_DD)
        this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                  this->limit);
    }
    this->PUSH_ENV_NOINDENT(tag->tagid);
    if (tag->parsedtag_exists(ATTR_COMPACT))
      envs[this->envc].env = HTML_DL_COMPACT;
    this->obuf.flag |= RB_IGNORE_P;
    return 1;
  }

  int HTML_LI_enter(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    this->parser.CLOSE_DT(&this->obuf, this);
    if (this->envc > 0) {
      Str *num;
      this->parser.flushline(this, envs[this->envc - 1].indent, 0, this->limit);
      envs[this->envc].count++;
      const char *p;
      if (tag->parsedtag_get_value(ATTR_VALUE, &p)) {
        int count = atoi(p);
        if (count > 0)
          envs[this->envc].count = count;
        else
          envs[this->envc].count = 0;
      }
      switch (envs[this->envc].env) {
      case HTML_UL: {
        envs[this->envc].type = tag->ul_type(envs[this->envc].type);
        for (int i = 0; i < INDENT_INCR - 3; i++)
          this->parser.push_charp(&this->obuf, 1, NBSP, PC_ASCII);
        auto tmp = Strnew();
        switch (envs[this->envc].type) {
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
          push_symbol(tmp, UL_SYMBOL((this->envc_real - 1) % MAX_UL_LEVEL),
                      symbol_width, 1);
          break;
        }
        if (symbol_width == 1)
          this->parser.push_charp(&this->obuf, 1, NBSP, PC_ASCII);
        this->parser.push_str(&this->obuf, symbol_width, tmp, PC_ASCII);
        this->parser.push_charp(&this->obuf, 1, NBSP, PC_ASCII);
        set_space_to_prevchar(this->obuf.prevchar);
        break;
      }

      case HTML_OL:
        if (tag->parsedtag_get_value(ATTR_TYPE, &p))
          envs[this->envc].type = (int)*p;
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
        this->parser.push_spaces(&this->obuf, 1, INDENT_INCR - num->length);
        this->parser.push_str(&this->obuf, num->length, num, PC_ASCII);
        if (INDENT_INCR >= 4)
          set_space_to_prevchar(this->obuf.prevchar);
        break;
      default:
        this->parser.push_spaces(&this->obuf, 1, INDENT_INCR);
        break;
      }
    } else {
      this->parser.flushline(this, 0, 0, this->limit);
    }
    this->obuf.flag |= RB_IGNORE_P;
    return 1;
  }

  int HTML_DT_enter() {
    this->parser.CLOSE_A(&this->obuf, this);
    if (this->envc == 0 || (this->envc_real < (int)this->envs.size() &&
                            envs[this->envc].env != HTML_DL &&
                            envs[this->envc].env != HTML_DL_COMPACT)) {
      this->PUSH_ENV_NOINDENT(HTML_DL);
    }
    if (this->envc > 0) {
      this->parser.flushline(this, envs[this->envc - 1].indent, 0, this->limit);
    }
    if (!(this->obuf.flag & RB_IN_DT)) {
      this->parser.HTMLlineproc1("<b>", this);
      this->obuf.flag |= RB_IN_DT;
    }
    this->obuf.flag |= RB_IGNORE_P;
    return 1;
  }

  int HTML_DT_exit() {
    if (!(this->obuf.flag & RB_IN_DT)) {
      return 1;
    }
    this->obuf.flag &= ~RB_IN_DT;
    this->parser.HTMLlineproc1("</b>", this);
    if (this->envc > 0 && envs[this->envc].env == HTML_DL)
      this->parser.flushline(this, envs[this->envc - 1].indent, 0, this->limit);
    return 1;
  }

  int HTML_DD_enter() {
    this->parser.CLOSE_A(&this->obuf, this);
    this->parser.CLOSE_DT(&this->obuf, this);
    if (envs[this->envc].env == HTML_DL ||
        envs[this->envc].env == HTML_DL_COMPACT) {
      this->PUSH_ENV(HTML_DD);
    }

    if (this->envc > 0 && envs[this->envc - 1].env == HTML_DL_COMPACT) {
      if (this->obuf.pos > envs[this->envc].indent)
        this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      else
        this->parser.push_spaces(&this->obuf, 1,
                                 envs[this->envc].indent - this->obuf.pos);
    } else
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    /* this->obuf.flag |= RB_IGNORE_P; */
    return 1;
  }

  int HTML_TITLE_enter(HtmlTag *tag) {
    this->parser.close_anchor(this);
    this->parser.process_title(tag);
    this->obuf.flag |= RB_TITLE;
    this->obuf.end_tag = HTML_N_TITLE;
    return 1;
  }

  int HTML_TITLE_exit(HtmlTag *tag) {
    if (!(this->obuf.flag & RB_TITLE))
      return 1;
    this->obuf.flag &= ~RB_TITLE;
    this->obuf.end_tag = 0;
    auto tmp = this->parser.process_n_title(tag);
    if (tmp)
      this->parser.HTMLlineproc1(tmp->ptr, this);
    return 1;
  }

  int HTML_TITLE_ALT(HtmlTag *tag) {
    const char *p;
    if (tag->parsedtag_get_value(ATTR_TITLE, &p))
      this->title = html_unquote(p);
    return 0;
  }

  int HTML_FRAMESET_enter(HtmlTag *tag) {
    this->PUSH_ENV(tag->tagid);
    this->parser.push_charp(&this->obuf, 9, "--FRAME--", PC_ASCII);
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    return 0;
  }

  int HTML_FRAMESET_exit() {
    if (this->envc > 0) {
      this->POP_ENV();
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    }
    return 0;
  }

  int HTML_NOFRAMES_enter() {
    this->parser.CLOSE_A(&this->obuf, this);
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    this->obuf.flag |= (RB_NOFRAMES | RB_IGNORE_P);
    /* istr = str; */
    return 1;
  }

  int HTML_NOFRAMES_exit() {
    this->parser.CLOSE_A(&this->obuf, this);
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    this->obuf.flag &= ~RB_NOFRAMES;
    return 1;
  }

  int HTML_FRAME(HtmlTag *tag) {
    const char *q = nullptr;
    const char *r = nullptr;
    tag->parsedtag_get_value(ATTR_SRC, &q);
    tag->parsedtag_get_value(ATTR_NAME, &r);
    if (q) {
      q = html_quote(q);
      this->parser.push_tag(
          &this->obuf,
          Sprintf("<a hseq=\"%d\" href=\"%s\">", this->parser.cur_hseq++, q)
              ->ptr,
          HTML_A);
      if (r)
        q = html_quote(r);
      this->parser.push_charp(&this->obuf, get_strwidth(q), q, PC_ASCII);
      this->parser.push_tag(&this->obuf, "</a>", HTML_N_A);
    }
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    return 0;
  }

  int HTML_HR(HtmlTag *tag) {
    this->parser.close_anchor(this);
    auto tmp =
        this->parser.process_hr(tag, this->limit, envs[this->envc].indent);
    this->parser.HTMLlineproc1(tmp->ptr, this);
    set_space_to_prevchar(this->obuf.prevchar);
    return 1;
  }

  int HTML_PRE_enter(HtmlTag *tag) {
    int x = tag->parsedtag_exists(ATTR_FOR_TABLE);
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      if (!x)
        this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                  this->limit);
    } else
      this->parser.fillline(&this->obuf, envs[this->envc].indent);
    this->obuf.flag |= (RB_PRE | RB_IGNORE_P);
    /* istr = str; */
    return 1;
  }

  int HTML_PRE_exit() {
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                this->limit);
      this->obuf.flag |= RB_IGNORE_P;
      this->blank_lines++;
    }
    this->obuf.flag &= ~RB_PRE;
    this->parser.close_anchor(this);
    return 1;
  }

  int HTML_PRE_INT_enter() {
    int i = this->obuf.line->length;
    this->parser.append_tags(&this->obuf);
    if (!(this->obuf.flag & RB_SPECIAL)) {
      this->obuf.set_breakpoint(this->obuf.line->length - i);
    }
    this->obuf.flag |= RB_PRE_INT;
    return 0;
  }

  int HTML_PRE_INT_exit() {
    this->parser.push_tag(&this->obuf, "</pre_int>", HTML_N_PRE_INT);
    this->obuf.flag &= ~RB_PRE_INT;
    if (!(this->obuf.flag & RB_SPECIAL) && this->obuf.pos > this->obuf.bp.pos) {
      set_prevchar(this->obuf.prevchar, "", 0);
      this->obuf.prev_ctype = PC_CTRL;
    }
    return 1;
  }

  int HTML_NOBR_enter() {
    this->obuf.flag |= RB_NOBR;
    this->obuf.nobr_level++;
    return 0;
  }

  int HTML_NOBR_exit() {
    if (this->obuf.nobr_level > 0)
      this->obuf.nobr_level--;
    if (this->obuf.nobr_level == 0)
      this->obuf.flag &= ~RB_NOBR;
    return 0;
  }

  int HTML_PRE_PLAIN_enter() {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                this->limit);
    }
    this->obuf.flag |= (RB_PRE | RB_IGNORE_P);
    return 1;
  }

  int HTML_PRE_PLAIN_exit() {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                this->limit);
      this->obuf.flag |= RB_IGNORE_P;
    }
    this->obuf.flag &= ~RB_PRE;
    return 1;
  }

  int HTML_PLAINTEXT_enter(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                this->limit);
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

  int HTML_LISTING_exit() {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
      this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                this->limit);
      this->obuf.flag |= RB_IGNORE_P;
    }
    this->obuf.flag &= ~RB_PLAIN;
    this->obuf.end_tag = 0;
    return 1;
  }

  int HTML_A_enter(HtmlTag *tag) {
    if (this->obuf.anchor.url.size()) {
      this->parser.close_anchor(this);
    }
    int hseq = 0;

    const char *p;
    if (tag->parsedtag_get_value(ATTR_HREF, &p))
      this->obuf.anchor.url = Strnew_charp(p)->ptr;
    if (tag->parsedtag_get_value(ATTR_TARGET, &p))
      this->obuf.anchor.target = Strnew_charp(p)->ptr;
    if (tag->parsedtag_get_value(ATTR_REFERER, &p))
      this->obuf.anchor.option = {.referer = p};
    if (tag->parsedtag_get_value(ATTR_TITLE, &p))
      this->obuf.anchor.title = Strnew_charp(p)->ptr;
    if (tag->parsedtag_get_value(ATTR_ACCESSKEY, &p))
      this->obuf.anchor.accesskey = (unsigned char)*p;
    if (tag->parsedtag_get_value(ATTR_HSEQ, &hseq))
      this->obuf.anchor.hseq = hseq;

    if (hseq == 0 && this->obuf.anchor.url.size()) {
      this->obuf.anchor.hseq = this->parser.cur_hseq;
      auto tmp = this->parser.process_anchor(tag, this->tagbuf->ptr);
      this->parser.push_tag(&this->obuf, tmp->ptr, HTML_A);
      return 1;
    }
    return 0;
  }

  int HTML_IMG_enter(HtmlTag *tag) {
    if (tag->parsedtag_exists(ATTR_USEMAP))
      this->parser.HTML5_CLOSE_A(&this->obuf, this);
    auto tmp = this->parser.process_img(tag, this->limit);
    if (this->parser.need_number) {
      tmp = Strnew_m_charp(this->parser.getLinkNumberStr(-1)->ptr, tmp->ptr,
                           nullptr);
      this->parser.need_number = 0;
    }
    this->parser.HTMLlineproc1(tmp->ptr, this);
    return 1;
  }

  int HTML_IMG_ALT_enter(HtmlTag *tag) {
    const char *p;
    if (tag->parsedtag_get_value(ATTR_SRC, &p))
      this->obuf.img_alt = Strnew_charp(p);
    return 0;
  }

  int HTML_IMG_ALT_exit() {
    if (this->obuf.img_alt) {
      if (!this->parser.close_effect0(&this->obuf, HTML_IMG_ALT))
        this->parser.push_tag(&this->obuf, "</img_alt>", HTML_N_IMG_ALT);
      this->obuf.img_alt = nullptr;
    }
    return 1;
  }

  int HTML_INPUT_ALT_enter(HtmlTag *tag) {
    int i = 0;
    if (tag->parsedtag_get_value(ATTR_TOP_MARGIN, &i)) {
      if ((short)i > this->obuf.top_margin)
        this->obuf.top_margin = (short)i;
    }
    i = 0;
    if (tag->parsedtag_get_value(ATTR_BOTTOM_MARGIN, &i)) {
      if ((short)i > this->obuf.bottom_margin)
        this->obuf.bottom_margin = (short)i;
    }
    int hseq;
    if (tag->parsedtag_get_value(ATTR_HSEQ, &hseq)) {
      this->obuf.input_alt.hseq = hseq;
    }
    if (tag->parsedtag_get_value(ATTR_FID, &i)) {
      this->obuf.input_alt.fid = i;
    }
    const char *p;
    if (tag->parsedtag_get_value(ATTR_TYPE, &p)) {
      this->obuf.input_alt.type = Strnew_charp(p);
    }
    if (tag->parsedtag_get_value(ATTR_VALUE, &p)) {
      this->obuf.input_alt.value = Strnew_charp(p);
    }
    if (tag->parsedtag_get_value(ATTR_NAME, &p)) {
      this->obuf.input_alt.name = Strnew_charp(p);
    }
    this->obuf.input_alt.in = 1;
    return 0;
  }

  int HTML_INPUT_ALT_exit() {
    if (this->obuf.input_alt.in) {
      if (!this->parser.close_effect0(&this->obuf, HTML_INPUT_ALT))
        this->parser.push_tag(&this->obuf, "</input_alt>", HTML_N_INPUT_ALT);
      this->obuf.input_alt.hseq = 0;
      this->obuf.input_alt.fid = -1;
      this->obuf.input_alt.in = 0;
      this->obuf.input_alt.type = nullptr;
      this->obuf.input_alt.name = nullptr;
      this->obuf.input_alt.value = nullptr;
    }
    return 1;
  }

  int HTML_TABLE_enter(HtmlTag *tag) {
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
    this->parser.tables[this->obuf.table_level] =
        table::begin_table(w, x, y, z);
    this->parser.table_mode[this->obuf.table_level].pre_mode = 0;
    this->parser.table_mode[this->obuf.table_level].indent_level = 0;
    this->parser.table_mode[this->obuf.table_level].nobr_level = 0;
    this->parser.table_mode[this->obuf.table_level].caption = 0;
    this->parser.table_mode[this->obuf.table_level].end_tag =
        0; /* HTML_UNKNOWN */
    this->parser.tables[this->obuf.table_level]->total_width = width;
    return 1;
  }

  int HTML_CENTER_enter() {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & (RB_PREMODE | RB_IGNORE_P)))
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    this->obuf.RB_SAVE_FLAG();
    if (DisableCenter) {
      this->obuf.RB_SET_ALIGN(RB_LEFT);
    } else {
      this->obuf.RB_SET_ALIGN(RB_CENTER);
    }
    return 1;
  }

  int HTML_CENTER_exit() {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_PREMODE)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    }
    this->obuf.RB_RESTORE_FLAG();
    return 1;
  }

  int HTML_DIV_enter(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    }
    set_alignment(&this->obuf, tag);
    return 1;
  }

  int HTML_DIV_exit() {
    this->parser.CLOSE_A(&this->obuf, this);
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    this->obuf.RB_RESTORE_FLAG();
    return 1;
  }

  int HTML_DIV_INT_enter(HtmlTag *tag) {
    this->parser.CLOSE_P(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P))
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    set_alignment(&this->obuf, tag);
    return 1;
  }

  int HTML_DIV_INT_exit() {
    this->parser.CLOSE_P(&this->obuf, this);
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    this->obuf.RB_RESTORE_FLAG();
    return 1;
  }

  int HTML_FORM_enter(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P))
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    auto tmp = this->parser.process_form(tag);
    if (tmp)
      this->parser.HTMLlineproc1(tmp->ptr, this);
    return 1;
  }

  int HTML_FORM_exit() {
    this->parser.CLOSE_A(&this->obuf, this);
    this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    this->obuf.flag |= RB_IGNORE_P;
    this->parser.process_n_form();
    return 1;
  }

  int HTML_INPUT_enter(HtmlTag *tag) {
    this->parser.close_anchor(this);
    auto tmp = this->parser.process_input(tag);
    if (tmp)
      this->parser.HTMLlineproc1(tmp->ptr, this);
    return 1;
  }

  int HTML_BUTTON_enter(HtmlTag *tag) {
    this->parser.HTML5_CLOSE_A(&this->obuf, this);
    auto tmp = this->parser.process_button(tag);
    if (tmp)
      this->parser.HTMLlineproc1(tmp->ptr, this);
    return 1;
  }

  int HTML_BUTTON_exit() {
    auto tmp = this->parser.process_n_button();
    if (tmp)
      this->parser.HTMLlineproc1(tmp->ptr, this);
    return 1;
  }

  int HTML_SELECT_enter(HtmlTag *tag) {
    this->parser.close_anchor(this);
    auto tmp = this->parser.process_select(tag);
    if (tmp)
      this->parser.HTMLlineproc1(tmp->ptr, this);
    this->obuf.flag |= RB_INSELECT;
    this->obuf.end_tag = HTML_N_SELECT;
    return 1;
  }

  int HTML_SELECT_exit() {
    this->obuf.flag &= ~RB_INSELECT;
    this->obuf.end_tag = 0;
    auto tmp = this->parser.process_n_select();
    if (tmp)
      this->parser.HTMLlineproc1(tmp->ptr, this);
    return 1;
  }

  int HTML_TEXTAREA_enter(HtmlTag *tag) {
    this->parser.close_anchor(this);
    auto tmp = this->parser.process_textarea(tag, this->limit);
    if (tmp) {
      this->parser.HTMLlineproc1(tmp->ptr, this);
    }
    this->obuf.flag |= RB_INTXTA;
    this->obuf.end_tag = HTML_N_TEXTAREA;
    return 1;
  }

  int HTML_TEXTAREA_exit() {
    this->obuf.flag &= ~RB_INTXTA;
    this->obuf.end_tag = 0;
    auto tmp = this->parser.process_n_textarea();
    if (tmp) {
      this->parser.HTMLlineproc1(tmp->ptr, this);
    }
    return 1;
  }

  int HTML_ISINDEX_enter(HtmlTag *tag) {
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

  int HTML_META_enter(HtmlTag *tag) {
    const char *p = nullptr;
    const char *q = nullptr;
    tag->parsedtag_get_value(ATTR_HTTP_EQUIV, &p);
    tag->parsedtag_get_value(ATTR_CONTENT, &q);
    if (p && q && !strcasecmp(p, "refresh")) {
      int refresh_interval;
      Str *tmp = nullptr;
      refresh_interval = getMetaRefreshParam(q, &tmp);
      if (tmp) {
        q = html_quote(tmp->ptr);
        tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
                      refresh_interval, q, q);
      } else if (refresh_interval > 0)
        tmp = Sprintf("Refresh (%d sec)", refresh_interval);
      if (tmp) {
        this->parser.HTMLlineproc1(tmp->ptr, this);
        this->parser.do_blankline(this, &this->obuf, envs[this->envc].indent, 0,
                                  this->limit);
      }
    }
    return 1;
  }

  int HTML_DEL_enter() {
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
        this->parser.push_tag(&this->obuf, "<s>", HTML_S);
      }
      break;
    }
    return 1;
  }

  int HTML_DEL_exit() {
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      this->obuf.flag &= ~RB_DEL;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      this->parser.HTMLlineproc1("<U>:DEL]</U>", this);
    case DISPLAY_INS_DEL_FONTIFY:
      if (this->obuf.fontstat.in_strike == 0)
        return 1;
      if (this->obuf.fontstat.in_strike == 1 &&
          this->parser.close_effect0(&this->obuf, HTML_S))
        this->obuf.fontstat.in_strike = 0;
      if (this->obuf.fontstat.in_strike > 0) {
        this->obuf.fontstat.in_strike--;
        if (this->obuf.fontstat.in_strike == 0) {
          this->parser.push_tag(&this->obuf, "</s>", HTML_N_S);
        }
      }
      break;
    }
    return 1;
  }

  int HTML_S_enter() {
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
        this->parser.push_tag(&this->obuf, "<s>", HTML_S);
      }
      break;
    }
    return 1;
  }

  int HTML_S_exit() {
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
      if (this->obuf.fontstat.in_strike == 1 &&
          this->parser.close_effect0(&this->obuf, HTML_S))
        this->obuf.fontstat.in_strike = 0;
      if (this->obuf.fontstat.in_strike > 0) {
        this->obuf.fontstat.in_strike--;
        if (this->obuf.fontstat.in_strike == 0) {
          this->parser.push_tag(&this->obuf, "</s>", HTML_N_S);
        }
      }
    }
    return 1;
  }

  int HTML_INS_enter() {
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
        this->parser.push_tag(&this->obuf, "<ins>", HTML_INS);
      }
      break;
    }
    return 1;
  }

  int HTML_INS_exit() {
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      break;
    case DISPLAY_INS_DEL_NORMAL:
      this->parser.HTMLlineproc1("<U>:INS]</U>", this);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (this->obuf.fontstat.in_ins == 0)
        return 1;
      if (this->obuf.fontstat.in_ins == 1 &&
          this->parser.close_effect0(&this->obuf, HTML_INS))
        this->obuf.fontstat.in_ins = 0;
      if (this->obuf.fontstat.in_ins > 0) {
        this->obuf.fontstat.in_ins--;
        if (this->obuf.fontstat.in_ins == 0) {
          this->parser.push_tag(&this->obuf, "</ins>", HTML_N_INS);
        }
      }
      break;
    }
    return 1;
  }

  int HTML_BGSOUND_enter(HtmlTag *tag) {
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

  int HTML_EMBED_enter(HtmlTag *tag) {
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

  int HTML_APPLET_enter(HtmlTag *tag) {
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

  int HTML_BODY_enter(HtmlTag *tag) {
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
};

int sloppy_parse_line(char **str);
