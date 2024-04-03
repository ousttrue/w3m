#pragma once
#include "html_command.h"
#include "html_readbuffer_flags.h"
#include <string>
#include <vector>
#include <optional>
#include <memory>

enum DisplayInsDelType {
  DISPLAY_INS_DEL_SIMPLE = 0,
  DISPLAY_INS_DEL_NORMAL = 1,
  DISPLAY_INS_DEL_FONTIFY = 2,
};

enum HtmlTagAttr {
  ATTR_UNKNOWN = 0,
  ATTR_ACCEPT = 1,
  ATTR_ACCEPT_CHARSET = 2,
  ATTR_ACTION = 3,
  ATTR_ALIGN = 4,
  ATTR_ALT = 5,
  ATTR_ARCHIVE = 6,
  ATTR_BACKGROUND = 7,
  ATTR_BORDER = 8,
  ATTR_CELLPADDING = 9,
  ATTR_CELLSPACING = 10,
  ATTR_CHARSET = 11,
  ATTR_CHECKED = 12,
  ATTR_COLS = 13,
  ATTR_COLSPAN = 14,
  ATTR_CONTENT = 15,
  ATTR_ENCTYPE = 16,
  ATTR_HEIGHT = 17,
  ATTR_HREF = 18,
  ATTR_HTTP_EQUIV = 19,
  ATTR_ID = 20,
  ATTR_LINK = 21,
  ATTR_MAXLENGTH = 22,
  ATTR_METHOD = 23,
  ATTR_MULTIPLE = 24,
  ATTR_NAME = 25,
  ATTR_NOWRAP = 26,
  ATTR_PROMPT = 27,
  ATTR_ROWS = 28,
  ATTR_ROWSPAN = 29,
  ATTR_SIZE = 30,
  ATTR_SRC = 31,
  ATTR_TARGET = 32,
  ATTR_TYPE = 33,
  ATTR_USEMAP = 34,
  ATTR_VALIGN = 35,
  ATTR_VALUE = 36,
  ATTR_VSPACE = 37,
  ATTR_WIDTH = 38,
  ATTR_COMPACT = 39,
  ATTR_START = 40,
  ATTR_SELECTED = 41,
  ATTR_LABEL = 42,
  ATTR_READONLY = 43,
  ATTR_SHAPE = 44,
  ATTR_COORDS = 45,
  ATTR_ISMAP = 46,
  ATTR_REL = 47,
  ATTR_REV = 48,
  ATTR_TITLE = 49,
  ATTR_ACCESSKEY = 50,
  ATTR_PUBLIC = 51,

  /* Internal attribute */
  ATTR_XOFFSET = 60,
  ATTR_YOFFSET = 61,
  ATTR_TOP_MARGIN = 62,
  ATTR_BOTTOM_MARGIN = 63,
  ATTR_TID = 64,
  ATTR_FID = 65,
  ATTR_FOR_TABLE = 66,
  ATTR_FRAMENAME = 67,
  ATTR_HBORDER = 68,
  ATTR_HSEQ = 69,
  ATTR_NO_EFFECT = 70,
  ATTR_REFERER = 71,
  ATTR_SELECTNUMBER = 72,
  ATTR_TEXTAREANUMBER = 73,
  ATTR_PRE_INT = 74,

  MAX_TAGATTR = 75,
};

class html_feed_environ;
class HtmlTag {
  friend std::shared_ptr<HtmlTag> parseHtmlTag(const char **s, bool internal);
  friend const char *parseAttr(const std::shared_ptr<HtmlTag> &tag,
                               const char *q, bool internal);

  std::vector<unsigned char> attrid;
  std::vector<std::string> value;
  std::vector<unsigned char> map;
  bool need_reconstruct = false;

public:
  const HtmlCommand tagid;
  HtmlTag(HtmlCommand id) : tagid(id) {}
  HtmlTag(const HtmlTag &) = delete;
  HtmlTag &operator=(const HtmlTag &) = delete;
  bool needRreconstruct() const { return this->need_reconstruct; }
  bool acceptsAttr(HtmlTagAttr id) const {
    return (this->map.size() && this->map[id] != MAX_TAGATTR);
  }
  bool existsAttr(HtmlTagAttr id) const {
    return (this->acceptsAttr(id) &&
            (this->attrid[this->map[id]] != ATTR_UNKNOWN));
  }
  std::optional<std::string> getAttr(HtmlTagAttr id) const;
  bool setAttr(HtmlTagAttr id, const std::string &value);
  std::string to_str() const;
  ReadBufferFlags alignFlag() const;

  int process(html_feed_environ *h_env);

private:
  int HTML_Paragraph(html_feed_environ *h_env);
  int HTML_H_enter(html_feed_environ *h_env);
  int HTML_H_exit(html_feed_environ *h_env);
  int HTML_List_enter(html_feed_environ *h_env);
  int HTML_List_exit(html_feed_environ *h_env);
  int HTML_DL_enter(html_feed_environ *h_env);
  int HTML_LI_enter(html_feed_environ *h_env);
  int HTML_DT_enter(html_feed_environ *h_env);
  int HTML_DT_exit(html_feed_environ *h_env);
  int HTML_DD_enter(html_feed_environ *h_env);
  int HTML_TITLE_enter(html_feed_environ *h_env);
  int HTML_TITLE_exit(html_feed_environ *h_env);
  int HTML_TITLE_ALT_enter(html_feed_environ *h_env);
  int HTML_FRAMESET_enter(html_feed_environ *h_env);
  int HTML_FRAMESET_exit(html_feed_environ *h_env);
  int HTML_NOFRAMES_enter(html_feed_environ *h_env);
  int HTML_NOFRAMES_exit(html_feed_environ *h_env);
  int HTML_FRAME_enter(html_feed_environ *h_env);
  int HTML_HR_enter(html_feed_environ *h_env);
  int HTML_PRE_enter(html_feed_environ *h_env);
  int HTML_PRE_exit(html_feed_environ *h_env);
  int HTML_PRE_PLAIN_enter(html_feed_environ *h_env);
  int HTML_PRE_PLAIN_exit(html_feed_environ *h_env);
  int HTML_PLAINTEXT_enter(html_feed_environ *h_env);
  int HTML_LISTING_exit(html_feed_environ *h_env);
  int HTML_A_enter(html_feed_environ *h_env);
  int HTML_IMG_enter(html_feed_environ *h_env);
  int HTML_IMG_ALT_enter(html_feed_environ *h_env);
  int HTML_IMG_ALT_exit(html_feed_environ *h_env);
  int HTML_TABLE_enter(html_feed_environ *h_env);
  int HTML_CENTER_enter(html_feed_environ *h_env);
  int HTML_CENTER_exit(html_feed_environ *h_env);
  int HTML_DIV_enter(html_feed_environ *h_env);
  int HTML_DIV_exit(html_feed_environ *h_env);
  int HTML_DIV_INT_enter(html_feed_environ *h_env);
  int HTML_DIV_INT_exit(html_feed_environ *h_env);
  int HTML_FORM_enter(html_feed_environ *h_env);
  int HTML_FORM_exit(html_feed_environ *h_env);
  int HTML_INPUT_enter(html_feed_environ *h_env);
  int HTML_BUTTON_enter(html_feed_environ *h_env);
  int HTML_BUTTON_exit(html_feed_environ *h_env);
  int HTML_SELECT_enter(html_feed_environ *h_env);
  int HTML_SELECT_exit(html_feed_environ *h_env);
  int HTML_TEXTAREA_enter(html_feed_environ *h_env);
  int HTML_TEXTAREA_exit(html_feed_environ *h_env);
  int HTML_ISINDEX_enter(html_feed_environ *h_env);
  int HTML_META_enter(html_feed_environ *h_env);
  int HTML_DEL_enter(html_feed_environ *h_env);
  int HTML_DEL_exit(html_feed_environ *h_env);
  int HTML_S_enter(html_feed_environ *h_env);
  int HTML_S_exit(html_feed_environ *h_env);
  int HTML_INS_enter(html_feed_environ *h_env);
  int HTML_INS_exit(html_feed_environ *h_env);
  int HTML_BGSOUND_enter(html_feed_environ *h_env);
  int HTML_EMBED_enter(html_feed_environ *h_env);
  int HTML_APPLET_enter(html_feed_environ *h_env);
  int HTML_BODY_enter(html_feed_environ *h_env);
  int HTML_INPUT_ALT_enter(html_feed_environ *h_env);
  int HTML_INPUT_ALT_exit(html_feed_environ *h_env);
};
