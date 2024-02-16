#pragma once
#include "htmlcommand.h"
#include "html.h"

/* Parsed Tag structure */

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

struct HtmlTag {
  HtmlCommand tagid;
  unsigned char *attrid;
  char **value;
  unsigned char *map;
  char need_reconstruct;

  static HtmlTag *parse(const char **s, int internal);
  bool parsedtag_accepts(HtmlTagAttr id) const {
    return (this->map && this->map[id] != MAX_TAGATTR);
  }
  bool parsedtag_exists(HtmlTagAttr id) const {
    return (this->parsedtag_accepts(id) &&
            (this->attrid[this->map[id]] != ATTR_UNKNOWN));
  }
};

#define parsedtag_delete(tag, id)                                              \
  (tag->parsedtag_accepts(id) && ((tag)->attrid[(tag)->map[id]] = ATTR_UNKNOWN))
#define parsedtag_need_reconstruct(tag) ((tag)->need_reconstruct)
#define parsedtag_attname(tag, i) (AttrMAP[(tag)->attrid[i]].name)

struct Str;
int parsedtag_get_value(HtmlTag *tag, HtmlTagAttr id, void *value);
int parsedtag_set_value(HtmlTag *tag, HtmlTagAttr id, char *value);
Str *parsedtag2str(HtmlTag *tag);

Str *process_img(HtmlTag *tag, int width);
Str *process_anchor(HtmlTag *tag, const char *tagbuf);
Str *process_input(HtmlTag *tag);
Str *process_button(HtmlTag *tag);
Str *process_n_button(void);
Str *process_select(HtmlTag *tag);
Str *process_n_select(void);
void feed_select(const char *str);
void process_option(void);
Str *process_textarea(HtmlTag *tag, int width);
Str *process_n_textarea(void);
void feed_textarea(const char *str);
Str *process_form(HtmlTag *tag);
Str *process_n_form(void);
int getMetaRefreshParam(const char *q, Str **refresh_uri);
