#pragma once
#include "url.h"
#include "html_command.h"
#include "lineprop.h"
#include "table.h"
#include "readbuffer_status.h"
#include "readbuffer.h"
#include "line.h"
#include "anchorlist.h"

struct Str;
class HtmlTag;

using FeedFunc = Str *(*)();

class LineFeed {
  std::shared_ptr<GeneralList> _tl_lp2;

public:
  LineFeed(const std::shared_ptr<GeneralList> &tl) : _tl_lp2(tl) {}

  const char *textlist_feed(void) {
    if (_tl_lp2 && _tl_lp2->_list.size()) {
      auto p = _tl_lp2->_list.front();
      _tl_lp2->_list.pop_front();
      return p->line->ptr;
    }
    return NULL;
  }
};

struct HttpResponse;
struct LineData;
struct FormAnchor;
struct Anchor;
class HtmlParser {
  friend struct html_feed_environ;

  std::shared_ptr<table> tables[MAX_TABLE];
  struct table_mode table_mode[MAX_TABLE];
  int table_width(struct html_feed_environ *h_env, int table_level);

  // title
  Str *pre_title = nullptr;
  Str *cur_title = nullptr;
  void process_title(const std::shared_ptr<HtmlTag> &tag);
  Str *process_n_title(const std::shared_ptr<HtmlTag> &tag);
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
  int _width;
  HtmlParser(int width);
  Str *process_select(const std::shared_ptr<HtmlTag> &tag);
  Str *process_n_select();
  void process_option();
  void feed_select(const char *str);

private:
  // form
  std::vector<std::shared_ptr<struct Form>> forms;
  std::vector<int> form_stack;
  int forms_size = 0;
  int form_sp = -1;

  // textarea
  Str *cur_textarea = {};
  int cur_textarea_size = {};
  int cur_textarea_rows = {};
  int cur_textarea_readonly = {};
  int n_textarea = -1;
  int ignore_nl_textarea = {};

public:
  std::vector<std::string> textarea_str;
  std::vector<struct FormAnchor *> a_textarea;

  Str *process_form_int(const std::shared_ptr<HtmlTag> &tag, int fid);
  Str *process_n_form();
  Str *process_form(const std::shared_ptr<HtmlTag> &tag) {
    return process_form_int(tag, -1);
  }
  int cur_form_id() const {
    return ((form_sp >= 0) ? form_stack[form_sp] : -1);
  }

  Str *process_textarea(const std::shared_ptr<HtmlTag> &tag, int width);
  Str *process_n_textarea();
  void feed_textarea(const char *str);
  void close_anchor(struct html_feed_environ *h_env);
  void save_fonteffect(html_feed_environ *h_env);
  void restore_fonteffect(html_feed_environ *h_env);
  void proc_escape(struct readbuffer *obuf, const char **str_return);
  void completeHTMLstream(html_feed_environ *);
  void push_render_image(Str *str, int width, int limit,
                         html_feed_environ *h_env);

public:
  int cur_hseq = 1;

  // HTML processing first pass
  void parse(std::string_view istr, struct html_feed_environ *h_env,
             bool internal);

  void HTMLlineproc1(const char *x, struct html_feed_environ *y);

  void CLOSE_DT(readbuffer *obuf, html_feed_environ *h_env);

  void CLOSE_A(readbuffer *obuf, html_feed_environ *h_env);

  void HTML5_CLOSE_A(readbuffer *obuf, html_feed_environ *h_env);

  Str *getLinkNumberStr(int correction) const;

  Str *process_img(const std::shared_ptr<HtmlTag> &tag, int width);
  std::string process_anchor(const std::shared_ptr<HtmlTag> &tag,
                             const char *tagbuf);
  Str *process_input(const std::shared_ptr<HtmlTag> &tag);
  Str *process_button(const std::shared_ptr<HtmlTag> &tag);
  Str *process_n_button();
  Str *process_hr(const std::shared_ptr<HtmlTag> &tag, int width,
                  int indent_width);

private:
  int pushHtmlTag(const std::shared_ptr<HtmlTag> &tag,
                  struct html_feed_environ *h_env);

  Lineprop effect = 0;
  Lineprop ex_effect = 0;
  char symbol = '\0';
  Anchor *a_href = nullptr;
  Anchor *a_img = nullptr;
  FormAnchor *a_form = nullptr;
  HtmlCommand internal = {};
  Line renderLine(const Url &url, html_feed_environ *h_env, LineData *data,
                  int nlines, const char *str, AnchorList<FormAnchor> *forms);

public:
  std::shared_ptr<LineLayout> render(const std::shared_ptr<LineLayout> &layout,
                                     const Url &currentUrl,
                                     html_feed_environ *h_env);
};

int getMetaRefreshParam(const char *q, Str **refresh_uri);
