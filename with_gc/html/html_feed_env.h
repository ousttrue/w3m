#pragma once
#include "html_parser.h"
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
  std::shared_ptr<GeneralList> buf;
  Str *tagbuf;
  int limit;

  std::vector<environment> envs;
  int envc = 0;

  int envc_real = 0;
  std::string title;

  HtmlParser parser;

  html_feed_environ(int nenv, int limit_width, int indent,
                    const std::shared_ptr<GeneralList> &_buf = {});
  void purgeline();

  void POP_ENV();

  void PUSH_ENV_NOINDENT(HtmlCommand cmd);

  void PUSH_ENV(HtmlCommand cmd);

  void parseLine(std::string_view istr, bool internal);

  void completeHTMLstream();

  std::shared_ptr<LineLayout> render(const std::shared_ptr<LineLayout> &layout,
                                     const Url &currentUrl);

  int HTML_Paragraph(const std::shared_ptr<HtmlTag> &tag);

  int HTML_H_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_H_exit();

  int HTML_List_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_List_exit(const std::shared_ptr<HtmlTag> &tag);

  int HTML_DL_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_LI_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_DT_enter();

  int HTML_DT_exit();

  int HTML_DD_enter();

  int HTML_TITLE_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_TITLE_exit(const std::shared_ptr<HtmlTag> &tag);

  int HTML_TITLE_ALT(const std::shared_ptr<HtmlTag> &tag);

  int HTML_FRAMESET_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_FRAMESET_exit();

  int HTML_NOFRAMES_enter();

  int HTML_NOFRAMES_exit();

  int HTML_FRAME(const std::shared_ptr<HtmlTag> &tag);

  int HTML_HR(const std::shared_ptr<HtmlTag> &tag);

  int HTML_PRE_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_PRE_exit();

  int HTML_PRE_PLAIN_enter();

  int HTML_PRE_PLAIN_exit();

  int HTML_PLAINTEXT_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_LISTING_exit();

  int HTML_A_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_IMG_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_IMG_ALT_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_IMG_ALT_exit();

  int HTML_TABLE_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_CENTER_enter();

  int HTML_CENTER_exit();

  int HTML_DIV_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_DIV_exit();

  int HTML_DIV_INT_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_DIV_INT_exit();

  int HTML_FORM_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_FORM_exit();

  int HTML_INPUT_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_BUTTON_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_BUTTON_exit();

  int HTML_SELECT_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_SELECT_exit();

  int HTML_TEXTAREA_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_TEXTAREA_exit();

  int HTML_ISINDEX_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_META_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_DEL_enter();

  int HTML_DEL_exit();

  int HTML_S_enter();

  int HTML_S_exit();

  int HTML_INS_enter();

  int HTML_INS_exit();

  int HTML_BGSOUND_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_EMBED_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_APPLET_enter(const std::shared_ptr<HtmlTag> &tag);

  int HTML_BODY_enter(const std::shared_ptr<HtmlTag> &tag);
};

#define MAX_ENV_LEVEL 20
inline void loadHTMLstream(const std::shared_ptr<LineLayout> &layout, int width,
                           const Url &currentURL, std::string_view body,
                           bool internal = false) {
  html_feed_environ htmlenv1(MAX_ENV_LEVEL, width, 0);
  htmlenv1.buf = GeneralList::newGeneralList();
  htmlenv1.parseLine(body, internal);
  htmlenv1.obuf.status = R_ST_NORMAL;
  htmlenv1.completeHTMLstream();
  htmlenv1.obuf.flushline(htmlenv1.buf, 0, 2, htmlenv1.limit);
  htmlenv1.render(layout, currentURL);
}
