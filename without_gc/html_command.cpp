#include "html_command.h"
#define MAX_CMD_LEN 128
#include "myctype.h"
#include "cmp.h"
#include <unordered_map>
#include <string>

std::unordered_map<std::string, HtmlCommand> tagtable = {
    /* 0 */ {"option_int", HTML_OPTION_INT},
    /* 1 */ {"figcaption", HTML_FIGCAPTION},
    /* 2 */ {"/form_int", HTML_N_FORM_INT},
    /* 3 */ {"/kbd", HTML_NOP},
    /* 4 */ {"dd", HTML_DD},
    /* 5 */ {"/dir", HTML_N_UL},
    /* 6 */ {"/body", HTML_N_BODY},
    /* 7 */ {"noframes", HTML_NOFRAMES},
    /* 8 */ {"bdo", HTML_BDO},
    /* 9 */ {"base", HTML_BASE},
    /* 10 */ {"/dt", HTML_N_DT},
    /* 11 */ {"/div", HTML_N_DIV},
    /* 12 */ {"big", HTML_BIG},
    /* 13 */ {"tbody", HTML_TBODY},
    /* 14 */ {"meta", HTML_META},
    /* 15 */ {"i", HTML_I},
    /* 16 */ {"label", HTML_LABEL},
    /* 17 */ {"/_symbol", HTML_N_SYMBOL},
    /* 18 */ {"sup", HTML_SUP},
    /* 19 */ {"/p", HTML_N_P},
    /* 20 */ {"/q", HTML_N_Q},
    /* 21 */ {"input_alt", HTML_INPUT_ALT},
    /* 22 */ {"dl", HTML_DL},
    /* 23 */ {"/tbody", HTML_N_TBODY},
    /* 24 */ {"/s", HTML_N_S},
    /* 25 */ {"/label", HTML_N_LABEL},
    /* 26 */ {"del", HTML_DEL},
    /* 27 */ {"xmp", HTML_XMP},
    /* 28 */ {"br", HTML_BR},
    /* 29 */ {"iframe", HTML_IFRAME},
    /* 30 */ {"link", HTML_LINK},
    /* 31 */ {"/u", HTML_N_U},
    /* 32 */ {"em", HTML_EM},
    /* 33 */ {"title_alt", HTML_TITLE_ALT},
    /* 34 */ {"caption", HTML_CAPTION},
    /* 35 */ {"plaintext", HTML_PLAINTEXT},
    /* 36 */ {"p", HTML_P},
    /* 37 */ {"q", HTML_Q},
    /* 38 */ {"blockquote", HTML_BLQ},
    /* 39 */ {"menu", HTML_UL},
    /* 40 */ {"fieldset", HTML_FIELDSET},
    /* 41 */ {"/colgroup", HTML_N_COLGROUP},
    /* 42 */ {"dfn", HTML_DFN},
    /* 43 */ {"s", HTML_S},
    /* 44 */ {"strong", HTML_STRONG},
    /* 45 */ {"dt", HTML_DT},
    /* 46 */ {"u", HTML_U},
    /* 47 */ {"/map", HTML_N_MAP},
    /* 48 */ {"/frameset", HTML_N_FRAMESET},
    /* 49 */ {"/ol", HTML_N_OL},
    /* 50 */ {"noscript", HTML_NOSCRIPT},
    /* 51 */ {"legend", HTML_LEGEND},
    /* 52 */ {"/td", HTML_N_TD},
    /* 53 */ {"li", HTML_LI},
    /* 54 */ {"html", HTML_BODY},
    /* 55 */ {"hr", HTML_HR},
    /* 56 */ {"/strong", HTML_N_STRONG},
    /* 57 */ {"small", HTML_SMALL},
    /* 58 */ {"/th", HTML_N_TH},
    /* 59 */ {"option", HTML_OPTION},
    /* 60 */ {"/span", HTML_N_SPAN},
    /* 61 */ {"kbd", HTML_NOP},
    /* 62 */ {"dir", HTML_UL},
    /* 63 */ {"col", HTML_COL},
    /* 64 */ {"param", HTML_PARAM},
    /* 65 */ {"/figcaption", HTML_N_FIGCAPTION},
    /* 66 */ {"/small", HTML_N_SMALL},
    /* 67 */ {"/legend", HTML_N_LEGEND},
    /* 68 */ {"/caption", HTML_N_CAPTION},
    /* 69 */ {"div", HTML_DIV},
    /* 70 */ {"/abbr", HTML_N_ABBR},
    /* 71 */ {"head", HTML_HEAD},
    /* 72 */ {"ol", HTML_OL},
    /* 73 */ {"/ul", HTML_N_UL},
    /* 74 */ {"/ins", HTML_N_INS},
    /* 75 */ {"area", HTML_AREA},
    /* 76 */ {"pre_plain", HTML_PRE_PLAIN},
    /* 77 */ {"button", HTML_BUTTON},
    /* 78 */ {"td", HTML_TD},
    /* 79 */ {"/option", HTML_N_OPTION},
    /* 80 */ {"/noframes", HTML_N_NOFRAMES},
    /* 81 */ {"/tr", HTML_N_TR},
    /* 82 */ {"nobr", HTML_NOBR},
    /* 83 */ {"img_alt", HTML_IMG_ALT},
    /* 84 */ {"table_alt", HTML_TABLE_ALT},
    /* 85 */ {"th", HTML_TH},
    /* 86 */ {"script", HTML_SCRIPT},
    /* 87 */ {"/tt", HTML_NOP},
    /* 88 */ {"code", HTML_NOP},
    /* 89 */ {"optgroup", HTML_OPTGROUP},
    /* 90 */ {"samp", HTML_NOP},
    /* 91 */ {"textarea", HTML_TEXTAREA},
    /* 92 */ {"textarea_int", HTML_TEXTAREA_INT},
    /* 93 */ {"/button", HTML_N_BUTTON},
    /* 94 */ {"table", HTML_TABLE},
    /* 95 */ {"img", HTML_IMG},
    /* 96 */ {"/blockquote", HTML_N_BLQ},
    /* 97 */ {"applet", HTML_APPLET},
    /* 98 */ {"map", HTML_MAP},
    /* 99 */ {"ul", HTML_UL},
    /* 100 */ {"basefont", HTML_BASEFONT},
    /* 101 */ {"/script", HTML_N_SCRIPT},
    /* 102 */ {"center", HTML_CENTER},
    /* 103 */ {"/table", HTML_N_TABLE},
    /* 104 */ {"cite", HTML_NOP},
    /* 105 */ {"/h1", HTML_N_H},
    /* 106 */ {"/fieldset", HTML_N_FIELDSET},
    /* 107 */ {"tr", HTML_TR},
    /* 108 */ {"/h2", HTML_N_H},
    /* 109 */ {"image", HTML_IMG},
    /* 110 */ {"/h3", HTML_N_H},
    /* 111 */ {"pre_int", HTML_PRE_INT},
    /* 112 */ {"/font", HTML_N_FONT},
    /* 113 */ {"tt", HTML_NOP},
    /* 114 */ {"/h4", HTML_N_H},
    /* 115 */ {"body", HTML_BODY},
    /* 116 */ {"/form", HTML_N_FORM},
    /* 117 */ {"/h5", HTML_N_H},
    /* 118 */ {"/h6", HTML_N_H},
    /* 119 */ {"frame", HTML_FRAME},
    /* 120 */ {"/textarea_int", HTML_N_TEXTAREA_INT},
    /* 121 */ {"/noscript", HTML_N_NOSCRIPT},
    /* 122 */ {"/img_alt", HTML_N_IMG_ALT},
    /* 123 */ {"/center", HTML_N_CENTER},
    /* 124 */ {"/pre", HTML_N_PRE},
    /* 125 */ {"tfoot", HTML_TFOOT},
    /* 126 */ {"ins", HTML_INS},
    /* 127 */ {"section", HTML_SECTION},
    /* 128 */ {"/var", HTML_NOP},
    /* 129 */ {"h1", HTML_H},
    /* 130 */ {"/tfoot", HTML_N_TFOOT},
    /* 131 */ {"input", HTML_INPUT},
    /* 132 */ {"h2", HTML_H},
    /* 133 */ {"h3", HTML_H},
    /* 134 */ {"h4", HTML_H},
    /* 135 */ {"h5", HTML_H},
    /* 136 */ {"internal", HTML_INTERNAL},
    /* 137 */ {"h6", HTML_H},
    /* 138 */ {"div_int", HTML_DIV_INT},
    /* 139 */ {"select_int", HTML_SELECT_INT},
    /* 140 */ {"/pre_int", HTML_N_PRE_INT},
    /* 141 */ {"figure", HTML_FIGURE},
    /* 142 */ {"/menu", HTML_N_UL},
    /* 143 */ {"form_int", HTML_FORM_INT},
    /* 144 */ {"/sub", HTML_N_SUB},
    /* 145 */ {"style", HTML_STYLE},
    /* 146 */ {"address", HTML_BR},
    /* 147 */ {"/optgroup", HTML_N_OPTGROUP},
    /* 148 */ {"/textarea", HTML_N_TEXTAREA},
    /* 149 */ {"/section", HTML_N_SECTION},
    /* 150 */ {"/input_alt", HTML_N_INPUT_ALT},
    /* 151 */ {"span", HTML_SPAN},
    /* 152 */ {"/figure", HTML_N_FIGURE},
    /* 153 */ {"doctype", HTML_DOCTYPE},
    /* 154 */ {"/style", HTML_N_STYLE},
    /* 155 */ {"/html", HTML_N_BODY},
    /* 156 */ {"pre", HTML_PRE},
    /* 157 */ {"title", HTML_TITLE},
    /* 158 */ {"abbr", HTML_ABBR},
    /* 159 */ {"select", HTML_SELECT},
    /* 160 */ {"/bdo", HTML_N_BDO},
    /* 161 */ {"acronym", HTML_ACRONYM},
    /* 162 */ {"/div_int", HTML_N_DIV_INT},
    /* 163 */ {"var", HTML_NOP},
    /* 164 */ {"/big", HTML_N_BIG},
    /* 165 */ {"/title", HTML_N_TITLE},
    /* 166 */ {"embed", HTML_EMBED},
    /* 167 */ {"/sup", HTML_N_SUP},
    /* 168 */ {"colgroup", HTML_COLGROUP},
    /* 169 */ {"/head", HTML_N_HEAD},
    /* 170 */ {"isindex", HTML_ISINDEX},
    /* 171 */ {"strike", HTML_S},
    /* 172 */ {"listing", HTML_LISTING},
    /* 173 */ {"bgsound", HTML_BGSOUND},
    /* 174 */ {"/address", HTML_BR},
    /* 175 */ {"object", HTML_OBJECT},
    /* 176 */ {"thead", HTML_THEAD},
    /* 177 */ {"wbr", HTML_WBR},
    /* 178 */ {"/del", HTML_N_DEL},
    /* 179 */ {"/nobr", HTML_N_NOBR},
    /* 180 */ {"/select", HTML_N_SELECT},
    /* 181 */ {"frameset", HTML_FRAMESET},
    /* 182 */ {"/xmp", HTML_N_XMP},
    /* 183 */ {"/dd", HTML_N_DD},
    /* 184 */ {"/code", HTML_NOP},
    /* 185 */ {"_symbol", HTML_SYMBOL},
    /* 186 */ {"/thead", HTML_N_THEAD},
    /* 187 */ {"/samp", HTML_NOP},
    /* 188 */ {"/dfn", HTML_N_DFN},
    /* 189 */ {"_id", HTML_NOP},
    /* 190 */ {"/strike", HTML_N_S},
    /* 191 */ {"/a", HTML_N_A},
    /* 192 */ {"/select_int", HTML_N_SELECT_INT},
    /* 193 */ {"sub", HTML_SUB},
    /* 194 */ {"/b", HTML_N_B},
    /* 195 */ {"/internal", HTML_N_INTERNAL},
    /* 196 */ {"/acronym", HTML_N_ACRONYM},
    /* 197 */ {"/pre_plain", HTML_N_PRE_PLAIN},
    /* 198 */ {"font", HTML_FONT},
    /* 199 */ {"/dl", HTML_N_DL},
    /* 200 */ {"form", HTML_FORM},
    /* 201 */ {"/cite", HTML_NOP},
    /* 202 */ {"a", HTML_A},
    /* 203 */ {"b", HTML_B},
    /* 204 */ {"/listing", HTML_N_LISTING},
    /* 205 */ {"/em", HTML_N_EM},
    /* 206 */ {"/i", HTML_N_I},
};
HtmlCommand gethtmlcmd(const char **s) {
  char cmdstr[MAX_CMD_LEN];
  const char *p = cmdstr;
  const char *save = *s;

  (*s)++;
  /* first character */
  if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  } else
    return HTML_UNKNOWN;
  if (p[-1] == '/')
    SKIP_BLANKS(*s);
  while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN) {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  }
  if (p - cmdstr == MAX_CMD_LEN) {
    /* buffer overflow: perhaps caused by bad HTML source */
    *s = save + 1;
    return HTML_UNKNOWN;
  }
  *(char *)p = '\0';

  /* hash search */
  auto found = tagtable.find(cmdstr);
  auto cmd = HTML_UNKNOWN;
  if (found != tagtable.end()) {
    cmd = found->second;
  }

  while (**s && **s != '>')
    (*s)++;
  if (**s == '>')
    (*s)++;
  return cmd;
}

bool toAlign(char *oval, AlignMode *align) {
  if (strcasecmp(oval, "left") == 0)
    *align = ALIGN_LEFT;
  else if (strcasecmp(oval, "right") == 0)
    *align = ALIGN_RIGHT;
  else if (strcasecmp(oval, "center") == 0)
    *align = ALIGN_CENTER;
  else if (strcasecmp(oval, "top") == 0)
    *align = ALIGN_TOP;
  else if (strcasecmp(oval, "bottom") == 0)
    *align = ALIGN_BOTTOM;
  else if (strcasecmp(oval, "middle") == 0)
    *align = ALIGN_MIDDLE;
  else
    return false;
  return true;
}
