#pragma once
#include "html_parser.h"
#include "html_command.h"
#include "html_tag.h"
#include "readbuffer.h"
#include "textline.h"
#include "generallist.h"
#include <stdio.h>
#include <vector>

#define MAX_INDENT_LEVEL 10

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

  html_feed_environ(int envc, int, int, GeneralList<TextLine> *_buf = nullptr);
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

  void render(HttpResponse *res, LineData *layout) {
    parser.render(res, this, layout);
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

  int HTML_DIV_enter(HtmlTag *tag) {
    this->parser.CLOSE_A(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P)) {
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    }
    set_alignment(&this->obuf, tag);
    return 1;
  }

  int HTML_DIV_INT_enter(HtmlTag *tag) {
    this->parser.CLOSE_P(&this->obuf, this);
    if (!(this->obuf.flag & RB_IGNORE_P))
      this->parser.flushline(this, envs[this->envc].indent, 0, this->limit);
    set_alignment(&this->obuf, tag);
    return 1;
  }
};

int sloppy_parse_line(char **str);
