#pragma once
#include <vector>
#include <functional>
#include "url.h"
#include "html_command.h"
#include "lineprop.h"
#include "html_table.h"
#include "anchorlist.h"
#include "readtoken.h"
#include "line_break.h"
#include <string>

#define MAX_INDENT_LEVEL 10

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

class html_feed_environ {
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

  // title
  std::string pre_title;
  std::string cur_title;
  HtmlCommand internal = {};

  void process_title();
  std::string process_n_title();
  void feed_title(const std::string &str);

  // push front
  // pop front
  std::list<std::shared_ptr<cmdtable>> tag_stack;
  std::list<std::shared_ptr<cmdtable>>::iterator find_stack(
      const std::function<bool(const std::shared_ptr<cmdtable> &)> &pred) {
    return std::find_if(tag_stack.begin(), tag_stack.end(), pred);
  }

  short top_margin = 0;
  short bottom_margin = 0;
  std::list<LinkStack> link_stack;
  int maxlimit = 0;
  int blank_lines = 0;

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

  void append_tags();
  void push_tag(std::string_view cmdname, HtmlCommand cmd);
  void push_nchars(int width, const char *str, int len, Lineprop mode);
  void push_charp(int width, const char *str, Lineprop mode) {
    this->push_nchars(width, str, strlen(str), mode);
  }
  void push_str(int width, std::string_view str, Lineprop mode) {
    this->push_nchars(width, str.data(), str.size(), mode);
  }
  void push_char(int pre_mode, char ch) {
    this->check_breakpoint(pre_mode, &ch);
    this->line += ch;
    this->pos++;
    this->prevchar = std::string(&ch, 1);
    if (ch != ' ')
      this->prev_ctype = PC_ASCII;
    this->flag |= RB_NFLUSHED;
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
  void proc_mchar(int pre_mode, int width, const char **str, Lineprop mode);
  void fillline(int indent) {
    this->push_spaces(1, indent - this->pos);
    this->flag &= ~RB_NFLUSHED;
  }

  // link
  void push_link(HtmlCommand cmd, int offset, int pos);
  const char *has_hidden_link(HtmlCommand cmd) const;
  void passthrough(const std::string &str, bool back);

  void flushline(const std::shared_ptr<GeneralList> &buf, int indent, int force,
                 int width);
  void do_blankline(const std::shared_ptr<GeneralList> &buf, int indent,
                    int indent_incr, int width) {
    if (this->blank_lines == 0) {
      this->flushline(buf, indent, 1, width);
    }
  }

  void parse_end(const std::shared_ptr<GeneralList> &buf, int limit,
                 int indent);
  std::shared_ptr<GeneralList> buf;
  std::string tagbuf;
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

public:
  std::shared_ptr<table> tables[MAX_TABLE];
  struct table_mode table_mode[MAX_TABLE];
  int table_width(int table_level);

private:
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

public:
  std::string process_select(const HtmlTag *tag);
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

public:
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

  void process_token(TableStatus &t, const struct Token &token);

public:
  int cur_hseq = 1;

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

public:
  html_feed_environ(int nenv, int limit_width, int indent,
                    const std::shared_ptr<GeneralList> &_buf = {})
      : buf(_buf), _width(limit_width) {
    assert(nenv);
    envs.resize(nenv);
    envs[0].indent = indent;

    this->prevchar = " ";
    this->flag = RB_IGNORE_P;
    this->status = R_ST_NORMAL;
    this->prev_ctype = PC_ASCII;
    this->bp.init_flag = 1;
    this->set_breakpoint(0);
  }
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
