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

class HtmlTag;
class html_feed_environ {
  friend struct table;
  friend struct tableimpl;
  friend struct Tokenizer;
  struct html_impl *_impl;
  friend class HtmlRenderer;

public:
  std::string line;
  Lineprop cprop = 0;
  std::string prevchar;
  ReadBufferFlags flag_stack[RB_STACK_SIZE];
  int flag_sp = 0;
  ReadBufferStatus status = {};
  HtmlCommand end_tag;
  unsigned char q_level = 0;
  short table_level = -1;
  Anchor anchor = {};

  html_feed_environ(int nenv, int limit_width, int indent,
                    const std::shared_ptr<GeneralList> &_buf = {});
  ~html_feed_environ();
  html_feed_environ(const html_feed_environ &) = delete;
  html_feed_environ &operator=(const html_feed_environ &) = delete;

  int pos()const;
  ReadBufferFlags flag() const;
  void addFlag(ReadBufferFlags flag);
  void removeFlag(ReadBufferFlags flag);
  ReadBufferFlags RB_GET_ALIGN() const;

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

  std::list<LinkStack> link_stack;
  int blank_lines = 0;

  void check_breakpoint(int pre_mode, const char *ch);

  void proc_mchar(int pre_mode, int width, const char **str, Lineprop mode);
  void push_link(HtmlCommand cmd, int offset, int pos);

  void passthrough(const std::string &str, bool back);

  const char *has_hidden_link(HtmlCommand cmd) const;

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

  Hidden pop_hidden();

  void flush_top_margin(FlushLineMode force);

  void flush_bottom_margin(FlushLineMode force);

  std::shared_ptr<TextLine> make_textline(int indent, int width);

  void flush_end(const Hidden &hidden, const std::string &pass);

  int _maxlimit = 0;

  void push_nchars(int width, const char *str, int len, Lineprop mode);

  int _cur_hseq = 1;

  void append_tags();

  void push_char(int pre_mode, char ch);

  void parse_end();

  std::shared_ptr<GeneralList> buf;

  std::string _tagbuf;

  const std::string &tagbuf() const { return _tagbuf; }
  void setToken(const std::string &str) { _tagbuf = str; }

  int append_token(const char **instr, bool pre);
  void appendGeneralList(const std::shared_ptr<GeneralList> &buf) {
    this->buf->appendGeneralList(buf);
  }

  int cur_hseq() const { return _cur_hseq; }
  int hseqAndIncrement() { return _cur_hseq++; }

public:
  LineFeed feed() { return LineFeed(this->buf); }

private:
  int maxlimit() const { return _maxlimit; }
  void setMaxLimit(int limit) {
    if (limit > this->_maxlimit) {
      this->_maxlimit = limit;
    }
  }
  void clearBlankLines() { this->blank_lines = 0; }
  void incrementBlankLines() { ++this->blank_lines; }

  void process_title();
  std::string process_n_title();
  void feed_title(const std::string &str);

  void RB_SAVE_FLAG();

  void RB_RESTORE_FLAG();

  void set_alignment(ReadBufferFlags flag);

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

  void push_spaces(int pre_mode, int width);

  void fillline();

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
  std::string _title;

public:
  int width() { return _width; }
  const std::string &title() const { return _title; }

private:
  std::shared_ptr<struct table> tables[MAX_TABLE];
  struct table_mode table_mode[MAX_TABLE];
  int table_width(int table_level);

  std::string process_select(const class std::shared_ptr<HtmlTag> &tag);
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

  std::string process_form_int(const std::shared_ptr<HtmlTag> &tag, int fid);
  std::string process_n_form();
  std::string process_form(const std::shared_ptr<HtmlTag> &tag) {
    return process_form_int(tag, -1);
  }
  int cur_form_id() const {
    return ((form_sp >= 0) ? form_stack[form_sp] : -1);
  }

  std::string process_textarea(const std::shared_ptr<HtmlTag> &tag, int width);
  std::string process_n_textarea();
  void feed_textarea(const std::string &str);
  void close_anchor();
  void save_fonteffect();
  void restore_fonteffect();
  void proc_escape(const char **str_return);

public:
  void completeHTMLstream();

  // HTML processing first pass
  void parse(std::string_view istr, bool internal = true);

private:
  void push_render_image(const std::string &str, int width, int limit);
  void process_token(struct TableStatus &t, const struct Token &token);

  void CLOSE_DT();
  void CLOSE_A();
  void HTML5_CLOSE_A();
  std::string getLinkNumberStr(int correction) const;
  std::string process_img(const std::shared_ptr<HtmlTag> &tag, int width);
  std::string process_anchor(const std::shared_ptr<HtmlTag> &tag,
                             const std::string &tagbuf);
  std::string process_input(const std::shared_ptr<HtmlTag> &tag);
  std::string process_button(const std::shared_ptr<HtmlTag> &tag);
  std::string process_n_button();
  std::string process_hr(const std::shared_ptr<HtmlTag> &tag, int width,
                         int indent_width);

  void purgeline();
  void POP_ENV();
  void PUSH_ENV_NOINDENT(HtmlCommand cmd);
  void PUSH_ENV(HtmlCommand cmd);

  std::shared_ptr<struct FormItem>
  createFormItem(const std::shared_ptr<HtmlTag> &tag);
  void make_caption(int table_width, const std::string &caption);

private:
  int process(const std::shared_ptr<HtmlTag> &tag);
  int HTML_Paragraph(const std::shared_ptr<HtmlTag> &tag);
  int HTML_H_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_H_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_List_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_List_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DL_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_LI_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DT_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DD_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_TITLE_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_TITLE_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_TITLE_ALT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_FRAMESET_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_FRAMESET_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_NOFRAMES_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_NOFRAMES_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_FRAME_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_HR_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_PRE_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_PRE_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_PRE_PLAIN_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_PRE_PLAIN_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_PLAINTEXT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_LISTING_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_A_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_IMG_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_IMG_ALT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_IMG_ALT_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_TABLE_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_CENTER_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_CENTER_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DIV_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DIV_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DIV_INT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DIV_INT_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_FORM_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_FORM_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_INPUT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_BUTTON_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_BUTTON_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_SELECT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_SELECT_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_TEXTAREA_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_TEXTAREA_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_ISINDEX_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_META_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DEL_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_DEL_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_S_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_S_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_INS_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_INS_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_BGSOUND_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_EMBED_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_APPLET_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_BODY_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_INPUT_ALT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_INPUT_ALT_exit(const std::shared_ptr<HtmlTag> &tag);

  int HTML_B_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_B_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_I_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_I_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_U_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_U_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_PRE_INT_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_PRE_INT_exit(const std::shared_ptr<HtmlTag> &tag);
  int HTML_NOBR_enter(const std::shared_ptr<HtmlTag> &tag);
  int HTML_NOBR_exit(const std::shared_ptr<HtmlTag> &tag);
};

#define MAX_ENV_LEVEL 20
struct LineData;
std::shared_ptr<LineData>
loadHTMLstream(int width, const Url &currentURL, std::string_view body,
               const std::shared_ptr<AnchorList<FormAnchor>> &old,
               bool internal = false);
