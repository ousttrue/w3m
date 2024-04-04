#pragma once
#include "url.h"
#include "html_command.h"
#include "lineprop.h"
#include "anchorlist.h"
#include "readtoken.h"
#include "line_break.h"
#include "generallist.h"
#include "html_readbuffer_flags.h"
#include "html_table_mode.h"
#include <string>
#include <vector>
#include <functional>

#define MAX_INDENT_LEVEL 10
#define MAX_TABLE 20 /* maximum nest level of table */

extern bool pseudoInlines;
extern bool ignore_null_img_alt;
extern int pixel_per_char_i;
extern bool displayLinkNumber;
extern bool DisableCenter;
extern int IndentIncr;
#define INDENT_INCR IndentIncr
extern bool DisplayBorders;
extern int displayInsDel;
extern bool view_unseenobject;
extern bool MetaRefresh;

#define RB_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define FONTSTAT_MAX 127

struct FontStat {
  char in_bold = 0;
  char in_under = 0;
  char in_italic = 0;
  char in_strike = 0;
  char in_ins = 0;
  char in_stand = 0;
};

struct input_alt_attr {
  int hseq = 0;
  int fid = -1;
  int in = 0;
  std::string type;
  std::string name;
  std::string value;
};

struct Breakpoint {
  int pos = 0;
  int len = 0;
  int tlen = 0;
  ReadBufferFlags flag = {};
  std::string img_alt;
  struct input_alt_attr input_alt = {};
  FontStat fontstat = {};
  short nobr_level = 0;
  Lineprop prev_ctype = 0;
  char init_flag = 1;
  short top_margin = 0;
  short bottom_margin = 0;
};

struct cmdtable {
  std::string cmdname;
  HtmlCommand cmd;
};

struct LinkStack {
  int cmd = 0;
  short offset = 0;
  short pos = 0;
};

enum class FlushLineMode {
  None,
  Force,
  Append,
};

class html_feed_environ {
  struct html_impl *_impl;

public:
  std::string line;
  Lineprop cprop = 0;
  short pos = 0;
  std::string prevchar;
  ReadBufferFlags flag = {};
  ReadBufferFlags flag_stack[RB_STACK_SIZE];
  int flag_sp = 0;
  ReadBufferStatus status = {};
  HtmlCommand end_tag;
  unsigned char q_level = 0;
  short table_level = -1;
  short nobr_level = 0;
  Anchor anchor = {};
  std::string img_alt;
  struct input_alt_attr input_alt = {};
  FontStat fontstat = {};
  FontStat fontstat_stack[FONT_STACK_SIZE];
  int fontstat_sp = 0;
  Lineprop prev_ctype;
  Breakpoint bp;

  html_feed_environ(int nenv, int limit_width, int indent,
                    const std::shared_ptr<GeneralList> &_buf = {});
  ~html_feed_environ();
  html_feed_environ(const html_feed_environ &) = delete;
  html_feed_environ &operator=(const html_feed_environ &) = delete;

private:
  // push front
  // pop front
  std::list<std::shared_ptr<cmdtable>> tag_stack;
  std::list<std::shared_ptr<cmdtable>>::iterator find_stack(
      const std::function<bool(const std::shared_ptr<cmdtable> &)> &pred) {
    return std::find_if(tag_stack.begin(), tag_stack.end(), pred);
  }

  // title
  std::string pre_title;
  std::string cur_title;
  HtmlCommand internal = {};

  // select
  std::string cur_select = {};
  std::string select_str = {};
  int select_is_multiple = {};
  int n_selectitem = {};
  std::string cur_option = {};
  std::string cur_option_value = {};
  std::string cur_option_label = {};
  int cur_option_selected = {};
  ReadBufferStatus cur_status = {};

  int top_margin = 0;
  int bottom_margin = 0;
  std::list<LinkStack> link_stack;
  int blank_lines = 0;

  void back_to_breakpoint() {
    this->flag = this->bp.flag;
    this->img_alt = this->bp.img_alt;
    this->input_alt = this->bp.input_alt;
    this->fontstat = this->bp.fontstat;
    this->prev_ctype = this->bp.prev_ctype;
    this->pos = this->bp.pos;
    this->top_margin = this->bp.top_margin;
    this->bottom_margin = this->bp.bottom_margin;
    if (this->flag & RB_NOBR) {
      this->nobr_level = this->bp.nobr_level;
    }
  }

  void check_breakpoint(int pre_mode, const char *ch) {
    int tlen;
    int len = this->line.size();

    this->append_tags();
    if (pre_mode)
      return;
    tlen = this->line.size() - len;
    if (tlen > 0 || is_boundary((unsigned char *)this->prevchar.c_str(),
                                (unsigned char *)ch))
      this->set_breakpoint(tlen);
  }

  void proc_mchar(int pre_mode, int width, const char **str, Lineprop mode);
  void push_link(HtmlCommand cmd, int offset, int pos);

  void passthrough(const std::string &str, bool back);

  const char *has_hidden_link(HtmlCommand cmd) const {
    if (line.back() != '>') {
      return nullptr;
    }
    auto p = std::find_if(link_stack.begin(), link_stack.end(),
                          [cmd, pos = this->pos](auto x) {
                            return x.cmd == cmd && x.pos == pos;
                          });
    if (p == link_stack.end()) {
      return nullptr;
    }
    return line.c_str() + p->offset;
  }

  struct Hidden {
    const char *str = nullptr;
    const char *anchor = nullptr;
    const char *img = nullptr;
    const char *input = nullptr;
    const char *bold = nullptr;
    const char *italic = nullptr;
    const char *under = nullptr;
    const char *strike = nullptr;
    const char *ins = nullptr;
    std::string suffix;
  };

  Hidden pop_hidden() {
    Hidden hidden;

    if (this->anchor.url.size()) {
      hidden.str = hidden.anchor = this->has_hidden_link(HTML_A);
    }
    if (this->img_alt.size()) {
      if ((hidden.img = this->has_hidden_link(HTML_IMG_ALT))) {
        if (!hidden.str || hidden.img < hidden.str)
          hidden.str = hidden.img;
      }
    }
    if (this->input_alt.in) {
      if ((hidden.input = this->has_hidden_link(HTML_INPUT_ALT))) {
        if (!hidden.str || hidden.input < hidden.str) {
          hidden.str = hidden.input;
        }
      }
    }
    if (this->fontstat.in_bold) {
      if ((hidden.bold = this->has_hidden_link(HTML_B))) {
        if (!hidden.str || hidden.bold < hidden.str) {
          hidden.str = hidden.bold;
        }
      }
    }
    if (this->fontstat.in_italic) {
      if ((hidden.italic = this->has_hidden_link(HTML_I))) {
        if (!hidden.str || hidden.italic < hidden.str) {
          hidden.str = hidden.italic;
        }
      }
    }
    if (this->fontstat.in_under) {
      if ((hidden.under = this->has_hidden_link(HTML_U))) {
        if (!hidden.str || hidden.under < hidden.str) {
          hidden.str = hidden.under;
        }
      }
    }
    if (this->fontstat.in_strike) {
      if ((hidden.strike = this->has_hidden_link(HTML_S))) {
        if (!hidden.str || hidden.strike < hidden.str) {
          hidden.str = hidden.strike;
        }
      }
    }
    if (this->fontstat.in_ins) {
      if ((hidden.ins = this->has_hidden_link(HTML_INS))) {
        if (!hidden.str || hidden.ins < hidden.str) {
          hidden.str = hidden.ins;
        }
      }
    }

    if (this->anchor.url.size() && !hidden.anchor)
      hidden.suffix += "</a>";
    if (this->img_alt.size() && !hidden.img)
      hidden.suffix += "</img_alt>";
    if (this->input_alt.in && !hidden.input)
      hidden.suffix += "</input_alt>";
    if (this->fontstat.in_bold && !hidden.bold)
      hidden.suffix += "</b>";
    if (this->fontstat.in_italic && !hidden.italic)
      hidden.suffix += "</i>";
    if (this->fontstat.in_under && !hidden.under)
      hidden.suffix += "</u>";
    if (this->fontstat.in_strike && !hidden.strike)
      hidden.suffix += "</s>";
    if (this->fontstat.in_ins && !hidden.ins)
      hidden.suffix += "</ins>";

    return hidden;
  }

  void flush_top_margin(FlushLineMode force) {
    if (this->top_margin <= 0) {
      return;
    }
    html_feed_environ h(1, this->_width, this->envs[this->envc].indent,
                        this->buf);
    h.pos = this->pos;
    h.flag = this->flag;
    h.top_margin = -1;
    h.bottom_margin = -1;
    h.line += "<pre_int>";
    for (int i = 0; i < h.pos; i++)
      h.line += ' ';
    h.line += "</pre_int>";
    for (int i = 0; i < this->top_margin; i++) {
      h.flushline(force);
    }
  }

  void flush_bottom_margin(FlushLineMode force) {
    if (this->bottom_margin <= 0) {
      return;
    }
    html_feed_environ h(1, this->_width, this->envs[this->envc].indent,
                        this->buf);
    h.pos = this->pos;
    h.flag = this->flag;
    h.top_margin = -1;
    h.bottom_margin = -1;
    h.line += "<pre_int>";
    for (int i = 0; i < h.pos; i++)
      h.line += ' ';
    h.line += "</pre_int>";
    for (int i = 0; i < this->bottom_margin; i++) {
      h.flushline(force);
    }
  }

  std::shared_ptr<TextLine> make_textline(int indent, int width);

  void flush_end(const Hidden &hidden, const std::string &pass);

  int _maxlimit = 0;

  void push_nchars(int width, const char *str, int len, Lineprop mode);

  int _cur_hseq = 1;

  void append_tags();

  void push_char(int pre_mode, char ch) {
    this->check_breakpoint(pre_mode, &ch);
    this->line += ch;
    this->pos++;
    this->prevchar = std::string(&ch, 1);
    if (ch != ' ')
      this->prev_ctype = PC_ASCII;
    this->flag |= RB_NFLUSHED;
  }

  void parse_end();

  std::shared_ptr<GeneralList> buf;

  std::string _tagbuf;

public:
  const std::string &tagbuf() const { return _tagbuf; }
  void setToken(const std::string &str) { _tagbuf = str; }
  int append_token(const char **instr, bool pre);
  void appendGeneralList(const std::shared_ptr<GeneralList> &buf) {
    this->buf->appendGeneralList(buf);
  }
  LineFeed feed() { return LineFeed(this->buf); }
  int cur_hseq() const { return _cur_hseq; }
  int hseqAndIncrement() { return _cur_hseq++; }
  int maxlimit() const { return _maxlimit; }
  void setMaxLimit(int limit) {
    if (limit > this->_maxlimit) {
      this->_maxlimit = limit;
    }
  }
  void clearBlankLines() { this->blank_lines = 0; }
  void incrementBlankLines() { ++this->blank_lines; }
  void setTopMargin(int i) {
    if (i > this->top_margin) {
      this->top_margin = i;
    }
  }
  void setBottomMargin(int i) {
    if (i > this->bottom_margin) {
      this->bottom_margin = i;
    }
  }

  void process_title();
  std::string process_n_title();
  void feed_title(const std::string &str);

  ReadBufferFlags RB_GET_ALIGN() const { return (this->flag & RB_ALIGN); }

  void RB_SET_ALIGN(ReadBufferFlags align) {
    this->flag &= ~RB_ALIGN;
    this->flag |= (align);
  }

  void RB_SAVE_FLAG() {
    if (this->flag_sp < RB_STACK_SIZE)
      this->flag_stack[this->flag_sp++] = RB_GET_ALIGN();
  }

  void RB_RESTORE_FLAG() {
    if (this->flag_sp > 0)
      RB_SET_ALIGN(this->flag_stack[--this->flag_sp]);
  }

  void set_alignment(ReadBufferFlags flag);
  void set_breakpoint(int tag_length);

  int close_effect0(HtmlCommand cmd) {
    auto found = std::find_if(tag_stack.begin(), tag_stack.end(),
                              [cmd](auto tag) { return tag->cmd == cmd; });
    if (found != tag_stack.end()) {
      tag_stack.erase(found);
      return 1;
    } else if (auto p = this->has_hidden_link(cmd)) {
      this->passthrough(p, 1);
      return 1;
    }
    return 0;
  }

  void CLOSE_P();

  void push_tag(std::string_view cmdname, HtmlCommand cmd);
  void push_charp(int width, const char *str, Lineprop mode) {
    this->push_nchars(width, str, strlen(str), mode);
  }
  void push_str(int width, std::string_view str, Lineprop mode) {
    this->push_nchars(width, str.data(), str.size(), mode);
  }

  void push_spaces(int pre_mode, int width) {
    if (width <= 0)
      return;
    this->check_breakpoint(pre_mode, " ");
    for (int i = 0; i < width; i++)
      this->line.push_back(' ');
    this->pos += width;
    this->prevchar = " ";
    this->flag |= RB_NFLUSHED;
  }

  void fillline() {
    this->push_spaces(1, this->envs[this->envc].indent - this->pos);
    this->flag &= ~RB_NFLUSHED;
  }

  void flushline(FlushLineMode force = {});

  void do_blankline() {
    if (this->blank_lines == 0) {
      this->flushline(FlushLineMode::Force);
    }
  }

  // int limit;
  int _width;
  struct environment {
    HtmlCommand env;
    int type;
    int count;
    char indent;
  };
  std::vector<environment> envs;
  int envc = 0;
  int envc_real = 0;
  std::string title;

  std::shared_ptr<struct table> tables[MAX_TABLE];
  struct table_mode table_mode[MAX_TABLE];
  int table_width(int table_level);

  std::string process_select(const class HtmlTag *tag);
  std::string process_n_select();
  void process_option();
  void feed_select(const std::string &str);

  // form
  std::vector<std::shared_ptr<struct Form>> forms;
  std::vector<int> form_stack;
  int forms_size = 0;
  int form_sp = -1;

  // textarea
  std::string cur_textarea = {};
  int cur_textarea_size = {};
  int cur_textarea_rows = {};
  int cur_textarea_readonly = {};
  int n_textarea = -1;
  int ignore_nl_textarea = {};

  std::vector<std::string> textarea_str;
  std::vector<struct FormAnchor *> a_textarea;

  std::string process_form_int(const HtmlTag *tag, int fid);
  std::string process_n_form();
  std::string process_form(const HtmlTag *tag) {
    return process_form_int(tag, -1);
  }
  int cur_form_id() const {
    return ((form_sp >= 0) ? form_stack[form_sp] : -1);
  }

  std::string process_textarea(const HtmlTag *tag, int width);
  std::string process_n_textarea();
  void feed_textarea(const std::string &str);
  void close_anchor();
  void save_fonteffect();
  void restore_fonteffect();
  void proc_escape(const char **str_return);
  void completeHTMLstream();
  void push_render_image(const std::string &str, int width, int limit);
  void process_token(struct TableStatus &t, const struct Token &token);

  // HTML processing first pass
  void parse(std::string_view istr, bool internal = true);
  void CLOSE_DT();
  void CLOSE_A();
  void HTML5_CLOSE_A();
  std::string getLinkNumberStr(int correction) const;
  std::string process_img(const HtmlTag *tag, int width);
  std::string process_anchor(HtmlTag *tag, const std::string &tagbuf);
  std::string process_input(const HtmlTag *tag);
  std::string process_button(const HtmlTag *tag);
  std::string process_n_button();
  std::string process_hr(const HtmlTag *tag, int width, int indent_width);

  void purgeline();
  void POP_ENV();
  void PUSH_ENV_NOINDENT(HtmlCommand cmd);
  void PUSH_ENV(HtmlCommand cmd);
  std::shared_ptr<struct FormItem>
  createFormItem(const std::shared_ptr<HtmlTag> &tag);
};

#define MAX_ENV_LEVEL 20
struct LineData;
std::shared_ptr<LineData>
loadHTMLstream(int width, const Url &currentURL, std::string_view body,
               const std::shared_ptr<AnchorList<FormAnchor>> &old,
               bool internal = false);
