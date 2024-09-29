#pragma once
#include "config.h"
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>

#include <time.h>

struct cmdtable {
  char *cmdname;
  int cmd;
};

#define MAILCAP_NEEDSTERMINAL 0x01
#define MAILCAP_COPIOUSOUTPUT 0x02
#define MAILCAP_HTMLOUTPUT 0x04

#define MCSTAT_REPNAME 0x01
#define MCSTAT_REPTYPE 0x02
#define MCSTAT_REPPARAM 0x04

struct table2 {
  char *item1;
  char *item2;
};

enum HtmlTag {
  HTML_UNKNOWN = 0,
  HTML_A = 1,
  HTML_N_A = 2,
  HTML_H = 3,
  HTML_N_H = 4,
  HTML_P = 5,
  HTML_BR = 6,
  HTML_B = 7,
  HTML_N_B = 8,
  HTML_UL = 9,
  HTML_N_UL = 10,
  HTML_LI = 11,
  HTML_OL = 12,
  HTML_N_OL = 13,
  HTML_TITLE = 14,
  HTML_N_TITLE = 15,
  HTML_HR = 16,
  HTML_DL = 17,
  HTML_N_DL = 18,
  HTML_DT = 19,
  HTML_DD = 20,
  HTML_PRE = 21,
  HTML_N_PRE = 22,
  HTML_BLQ = 23,
  HTML_N_BLQ = 24,
  HTML_IMG = 25,
  HTML_LISTING = 26,
  HTML_N_LISTING = 27,
  HTML_XMP = 28,
  HTML_N_XMP = 29,
  HTML_PLAINTEXT = 30,
  HTML_TABLE = 31,
  HTML_N_TABLE = 32,
  HTML_META = 33,
  HTML_N_P = 34,
  HTML_FRAME = 35,
  HTML_FRAMESET = 36,
  HTML_N_FRAMESET = 37,
  HTML_CENTER = 38,
  HTML_N_CENTER = 39,
  HTML_FONT = 40,
  HTML_N_FONT = 41,
  HTML_FORM = 42,
  HTML_N_FORM = 43,
  HTML_INPUT = 44,
  HTML_TEXTAREA = 45,
  HTML_N_TEXTAREA = 46,
  HTML_SELECT = 47,
  HTML_N_SELECT = 48,
  HTML_OPTION = 49,
  HTML_NOBR = 50,
  HTML_N_NOBR = 51,
  HTML_DIV = 52,
  HTML_N_DIV = 53,
  HTML_ISINDEX = 54,
  HTML_MAP = 55,
  HTML_N_MAP = 56,
  HTML_AREA = 57,
  HTML_SCRIPT = 58,
  HTML_N_SCRIPT = 59,
  HTML_BASE = 60,
  HTML_DEL = 61,
  HTML_N_DEL = 62,
  HTML_INS = 63,
  HTML_N_INS = 64,
  HTML_U = 65,
  HTML_N_U = 66,
  HTML_STYLE = 67,
  HTML_N_STYLE = 68,
  HTML_WBR = 69,
  HTML_EM = 70,
  HTML_N_EM = 71,
  HTML_BODY = 72,
  HTML_N_BODY = 73,
  HTML_TR = 74,
  HTML_N_TR = 75,
  HTML_TD = 76,
  HTML_N_TD = 77,
  HTML_CAPTION = 78,
  HTML_N_CAPTION = 79,
  HTML_TH = 80,
  HTML_N_TH = 81,
  HTML_THEAD = 82,
  HTML_N_THEAD = 83,
  HTML_TBODY = 84,
  HTML_N_TBODY = 85,
  HTML_TFOOT = 86,
  HTML_N_TFOOT = 87,
  HTML_COLGROUP = 88,
  HTML_N_COLGROUP = 89,
  HTML_COL = 90,
  HTML_BGSOUND = 91,
  HTML_APPLET = 92,
  HTML_EMBED = 93,
  HTML_N_OPTION = 94,
  HTML_HEAD = 95,
  HTML_N_HEAD = 96,
  HTML_DOCTYPE = 97,
  HTML_NOFRAMES = 98,
  HTML_N_NOFRAMES = 99,
  HTML_SUP = 100,
  HTML_N_SUP = 101,
  HTML_SUB = 102,
  HTML_N_SUB = 103,
  HTML_LINK = 104,
  HTML_S = 105,
  HTML_N_S = 106,
  HTML_Q = 107,
  HTML_N_Q = 108,
  HTML_I = 109,
  HTML_N_I = 110,
  HTML_STRONG = 111,
  HTML_N_STRONG = 112,
  HTML_SPAN = 113,
  HTML_N_SPAN = 114,
  HTML_ABBR = 115,
  HTML_N_ABBR = 116,
  HTML_ACRONYM = 117,
  HTML_N_ACRONYM = 118,
  HTML_BASEFONT = 119,
  HTML_BDO = 120,
  HTML_N_BDO = 121,
  HTML_BIG = 122,
  HTML_N_BIG = 123,
  HTML_BUTTON = 124,
  HTML_N_BUTTON = 125,
  HTML_FIELDSET = 126,
  HTML_N_FIELDSET = 127,
  HTML_IFRAME = 128,
  HTML_LABEL = 129,
  HTML_N_LABEL = 130,
  HTML_LEGEND = 131,
  HTML_N_LEGEND = 132,
  HTML_NOSCRIPT = 133,
  HTML_N_NOSCRIPT = 134,
  HTML_OBJECT = 135,
  HTML_OPTGROUP = 136,
  HTML_N_OPTGROUP = 137,
  HTML_PARAM = 138,
  HTML_SMALL = 139,
  HTML_N_SMALL = 140,
  HTML_FIGURE = 141,
  HTML_N_FIGURE = 142,
  HTML_FIGCAPTION = 143,
  HTML_N_FIGCAPTION = 144,
  HTML_SECTION = 145,
  HTML_N_SECTION = 146,
  HTML_N_DT = 147,
  HTML_N_DD = 148,

  /* pseudo tag */
  HTML_SELECT_INT = 160,
  HTML_N_SELECT_INT = 161,
  HTML_OPTION_INT = 162,
  HTML_TEXTAREA_INT = 163,
  HTML_N_TEXTAREA_INT = 164,
  HTML_TABLE_ALT = 165,
  HTML_SYMBOL = 166,
  HTML_N_SYMBOL = 167,
  HTML_PRE_INT = 168,
  HTML_N_PRE_INT = 169,
  HTML_TITLE_ALT = 170,
  HTML_FORM_INT = 171,
  HTML_N_FORM_INT = 172,
  HTML_DL_COMPACT = 173,
  HTML_INPUT_ALT = 174,
  HTML_N_INPUT_ALT = 175,
  HTML_IMG_ALT = 176,
  HTML_N_IMG_ALT = 177,
  HTML_NOP = 178,
  HTML_PRE_PLAIN = 179,
  HTML_N_PRE_PLAIN = 180,
  HTML_INTERNAL = 181,
  HTML_N_INTERNAL = 182,
  HTML_DIV_INT = 183,
  HTML_N_DIV_INT = 184,

  MAX_HTMLTAG = 185,
};

/* Tag attribute */

#define ATTR_UNKNOWN 0
#define ATTR_ACCEPT 1
#define ATTR_ACCEPT_CHARSET 2
#define ATTR_ACTION 3
#define ATTR_ALIGN 4
#define ATTR_ALT 5
#define ATTR_ARCHIVE 6
#define ATTR_BACKGROUND 7
#define ATTR_BORDER 8
#define ATTR_CELLPADDING 9
#define ATTR_CELLSPACING 10
#define ATTR_CHARSET 11
#define ATTR_CHECKED 12
#define ATTR_COLS 13
#define ATTR_COLSPAN 14
#define ATTR_CONTENT 15
#define ATTR_ENCTYPE 16
#define ATTR_HEIGHT 17
#define ATTR_HREF 18
#define ATTR_HTTP_EQUIV 19
#define ATTR_ID 20
#define ATTR_LINK 21
#define ATTR_MAXLENGTH 22
#define ATTR_METHOD 23
#define ATTR_MULTIPLE 24
#define ATTR_NAME 25
#define ATTR_NOWRAP 26
#define ATTR_PROMPT 27
#define ATTR_ROWS 28
#define ATTR_ROWSPAN 29
#define ATTR_SIZE 30
#define ATTR_SRC 31
#define ATTR_TARGET 32
#define ATTR_TYPE 33
#define ATTR_USEMAP 34
#define ATTR_VALIGN 35
#define ATTR_VALUE 36
#define ATTR_VSPACE 37
#define ATTR_WIDTH 38
#define ATTR_COMPACT 39
#define ATTR_START 40
#define ATTR_SELECTED 41
#define ATTR_LABEL 42
#define ATTR_READONLY 43
#define ATTR_SHAPE 44
#define ATTR_COORDS 45
#define ATTR_ISMAP 46
#define ATTR_REL 47
#define ATTR_REV 48
#define ATTR_TITLE 49
#define ATTR_ACCESSKEY 50
#define ATTR_PUBLIC 51

/* Internal attribute */
#define ATTR_XOFFSET 60
#define ATTR_YOFFSET 61
#define ATTR_TOP_MARGIN 62
#define ATTR_BOTTOM_MARGIN 63
#define ATTR_TID 64
#define ATTR_FID 65
#define ATTR_FOR_TABLE 66
#define ATTR_FRAMENAME 67
#define ATTR_HBORDER 68
#define ATTR_HSEQ 69
#define ATTR_NO_EFFECT 70
#define ATTR_REFERER 71
#define ATTR_SELECTNUMBER 72
#define ATTR_TEXTAREANUMBER 73
#define ATTR_PRE_INT 74

#define MAX_TAGATTR 75

/* HTML Tag Information Table */

typedef struct html_tag_info {
  char *name;
  unsigned char *accept_attribute;
  unsigned char max_attribute;
  unsigned char flag;
} TagInfo;

#define TFLG_END 1
#define TFLG_INT 2

/* HTML Tag Attribute Information Table */

typedef struct tag_attribute_info {
  char *name;
  unsigned char vtype;
  unsigned char flag;
} TagAttrInfo;

#define AFLG_INT 1

#define VTYPE_NONE 0
#define VTYPE_STR 1
#define VTYPE_NUMBER 2
#define VTYPE_LENGTH 3
#define VTYPE_ALIGN 4
#define VTYPE_VALIGN 5
#define VTYPE_ACTION 6
#define VTYPE_ENCTYPE 7
#define VTYPE_METHOD 8
#define VTYPE_MLENGTH 9
#define VTYPE_TYPE 10

#define SHAPE_UNKNOWN 0
#define SHAPE_DEFAULT 1
#define SHAPE_RECT 2
#define SHAPE_CIRCLE 3
#define SHAPE_POLY 4

extern TagInfo TagMAP[];
extern TagAttrInfo AttrMAP[];

struct environment {
  unsigned char env;
  int type;
  int count;
  char indent;
};

#define MAX_ENV_LEVEL 20
#define MAX_INDENT_LEVEL 10

#define INDENT_INCR IndentIncr

extern enum HtmlTag gethtmlcmd(char **s);
