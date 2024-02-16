#pragma once
#include "lineprop.h"

struct Str;
struct HtmlTag;

struct link_stack {
  int cmd = 0;
  short offset = 0;
  short pos = 0;
  link_stack *next = nullptr;
};

class HtmlParser {

  int need_number = 0;

  // link
  struct link_stack *link_stack = nullptr;
  char *has_hidden_link(struct readbuffer *obuf, int cmd) const;
  void passthrough(struct readbuffer *obuf, char *str, int back);
  void append_tags(struct readbuffer *obuf);

  // title
  Str *pre_title = nullptr;
  Str *cur_title = nullptr;
  void process_title(HtmlTag *tag);
  Str *process_n_title(HtmlTag *tag);
  void feed_title(const char *str);

  // select
  Str *cur_select = {};
  Str *select_str = {};
  int select_is_multiple = {};
  int n_selectitem = {};
  Str *cur_option = {};
  Str *cur_option_value = {};
  Str *cur_option_label = {};
  int cur_option_selected = {};
  int cur_status = {};

public:
  HtmlParser();
  Str *process_select(HtmlTag *tag);
  Str *process_n_select();
  void process_option();
  void feed_select(const char *str);

  // Str *process_select(HtmlTag *tag);
  // void process_option(HtmlParser *parser);
  // Str *process_textarea(HtmlTag *tag, int width);
  // Str *process_n_textarea(HtmlParser *parser);
  // void feed_textarea(const char *str);
  // Str *process_form(HtmlTag *tag);
  // Str *process_n_form(void);

private:
  // form
  struct FormList **forms = {};
  int *form_stack = {};
  int form_max = -1;
  int forms_size = 0;
  int form_sp = -1;

  // textarea
  Str *cur_textarea = {};
  int cur_textarea_size = {};
  int cur_textarea_rows = {};
  int cur_textarea_readonly = {};
  int n_textarea = 0;
  int ignore_nl_textarea = {};

public:
  Str **textarea_str = {};
  int max_textarea = 10;
  Str *process_form_int(struct HtmlTag *tag, int fid);
  Str *process_n_form();
  Str *process_form(struct HtmlTag *tag) { return process_form_int(tag, -1); }
  int cur_form_id() const {
    return ((form_sp >= 0) ? form_stack[form_sp] : -1);
  }

  Str *process_textarea(struct HtmlTag *tag, int width);
  Str *process_n_textarea();
  void feed_textarea(const char *str);

  void push_tag(struct readbuffer *obuf, const char *cmdname, int cmd);

  void push_nchars(struct readbuffer *obuf, int width, const char *str, int len,
                   Lineprop mode);
  void push_charp(readbuffer *obuf, int width, const char *str, Lineprop mode);
  void push_str(readbuffer *obuf, int width, Str *str, Lineprop mode);
  void check_breakpoint(struct readbuffer *obuf, int pre_mode, const char *ch);
  void push_char(struct readbuffer *obuf, int pre_mode, char ch);
  void push_spaces(struct readbuffer *obuf, int pre_mode, int width);
  void proc_mchar(struct readbuffer *obuf, int pre_mode, int width,
                  const char **str, Lineprop mode);
  void fillline(struct readbuffer *obuf, int indent);
  void flushline(struct html_feed_environ *h_env, readbuffer *obuf, int indent,
                 int force, int width);
  int close_effect0(struct readbuffer *obuf, int cmd);
  void close_anchor(struct html_feed_environ *h_env, struct readbuffer *obuf);
  void save_fonteffect(struct html_feed_environ *h_env,
                       struct readbuffer *obuf);
  void restore_fonteffect(struct html_feed_environ *h_env,
                          struct readbuffer *obuf);
  void proc_escape(struct readbuffer *obuf, const char **str_return);

  void completeHTMLstream(struct html_feed_environ *, struct readbuffer *);

  void push_render_image(Str *str, int width, int limit,
                         html_feed_environ *h_env);
  void do_blankline(struct html_feed_environ *h_env, struct readbuffer *obuf,
                    int indent, int indent_incr, int width);

public:
  void push_link(int cmd, int offset, int pos);
  int cur_hseq = 1;
  void HTMLlineproc0(const char *istr, struct html_feed_environ *h_env,
                     int internal);

  void HTMLlineproc1(const char *x, struct html_feed_environ *y) {
    HTMLlineproc0(x, y, true);
  }

  Str *getLinkNumberStr(int correction) const;

  void HTMLlineproc2body(struct HttpResponse *res, struct LineLayout *layout,
                         Str *(*feed)(), int llimit);

private:
  int HTMLtagproc1(HtmlTag *tag, struct html_feed_environ *h_env);
};
