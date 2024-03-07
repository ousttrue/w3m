#pragma once
#include "lineprop.h"
#include "table.h"
#include "readbuffer_status.h"

#define MAX_INDENT_LEVEL 10

struct Str;
struct HtmlTag;

struct link_stack {
  int cmd = 0;
  short offset = 0;
  short pos = 0;
  link_stack *next = nullptr;
};

using FeedFunc = Str *(*)();

class LineFeed {
  GeneralList<TextLine>::ListItem *_tl_lp2 = nullptr;

public:
  LineFeed(GeneralList<TextLine> *tl) : _tl_lp2(tl->first) {}

  const char *textlist_feed(void) {
    TextLine *p;
    if (_tl_lp2 != NULL) {
      p = _tl_lp2->ptr;
      _tl_lp2 = _tl_lp2->next;
      return p->line->ptr;
    }
    return NULL;
  }
};

class HtmlParser {
  int need_number = 0;

  struct table *tables[MAX_TABLE];
  struct table_mode table_mode[MAX_TABLE];
  int table_width(struct html_feed_environ *h_env, int table_level);

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
  ReadBufferStatus cur_status = {};

public:
  HtmlParser();
  Str *process_select(HtmlTag *tag);
  Str *process_n_select();
  void process_option();
  void feed_select(const char *str);

private:
  // form
  std::vector<struct FormList *> forms;
  std::vector<int> form_stack;
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
  std::vector<std::string> textarea_str;
  std::vector<struct FormAnchor *> a_textarea;

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
  void flushline(html_feed_environ *h_env, int indent, int force, int width);
  int close_effect0(struct readbuffer *obuf, int cmd);
  void close_anchor(struct html_feed_environ *h_env);
  void save_fonteffect(html_feed_environ *h_env);
  void restore_fonteffect(html_feed_environ *h_env);
  void proc_escape(struct readbuffer *obuf, const char **str_return);
  void completeHTMLstream(html_feed_environ *);
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

  Str *process_img(HtmlTag *tag, int width);
  Str *process_anchor(HtmlTag *tag, const char *tagbuf);
  Str *process_input(HtmlTag *tag);
  Str *process_button(HtmlTag *tag);
  Str *process_n_button();
  Str *process_hr(struct HtmlTag *tag, int width, int indent_width);

private:
  int HTMLtagproc1(HtmlTag *tag, struct html_feed_environ *h_env);

  std::vector<char> outc;
  std::vector<Lineprop> outp;

  void PPUSH(char p, Lineprop c, int *pos) {
    outp[*pos] = (p);
    outc[*pos] = (c);
    (*pos)++;
  }

  void PSIZE(int pos) {
    if (outc.size() <= pos + 1) {
      auto out_size = pos * 3 / 2;
      outc.resize(out_size);
      outp.resize(out_size);
    }
  }

public:
  void render(struct HttpResponse *res, struct LineData *layout, LineFeed *tl);
};

int getMetaRefreshParam(const char *q, Str **refresh_uri);
