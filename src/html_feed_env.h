#pragma once
#include "html_parser.h"
#include <vector>

#define MAX_INDENT_LEVEL 10
#define MAX_UL_LEVEL 9
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000

struct environment {
  HtmlCommand env;
  int type;
  int count;
  char indent;
};

struct readbuffer;
struct html_feed_environ {
  readbuffer obuf;
  std::shared_ptr<GeneralList> buf;
  std::string tagbuf;
  int limit;
  std::vector<environment> envs;
  int envc = 0;
  int envc_real = 0;
  std::string title;
  HtmlParser parser;

  html_feed_environ(int nenv, int limit_width, int indent,
                    const std::shared_ptr<GeneralList> &_buf = {})
      : buf(_buf), limit(limit_width), parser(limit_width) {
    assert(nenv);
    envs.resize(nenv);
    envs[0].indent = indent;
  }
  void purgeline();
  void POP_ENV();
  void PUSH_ENV_NOINDENT(HtmlCommand cmd);
  void PUSH_ENV(HtmlCommand cmd);
  void parseLine(std::string_view istr, bool internal);
  void completeHTMLstream();
  std::shared_ptr<LineData>
  render(const Url &currentUrl,
         const std::shared_ptr<AnchorList<FormAnchor>> &old);
  std::shared_ptr<struct FormItem>
  createFormItem(const std::shared_ptr<HtmlTag> &tag);
};

#define MAX_ENV_LEVEL 20
std::shared_ptr<LineData>
loadHTMLstream(int width, const Url &currentURL, std::string_view body,
               const std::shared_ptr<AnchorList<FormAnchor>> &old,
               bool internal = false);
