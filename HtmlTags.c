#include "HtmlTags.h"
#include <gcstr/gcstr.h>

#define ARR_SZ(arr) (sizeof(arr) / sizeof(arr[0]))

#define ATTR_CORE ATTR_ID
enum HtmlTagAttributes ALST_ID[] = { ATTR_CORE };
enum HtmlTagAttributes ALST_A[] = {
    ATTR_NAME, ATTR_HREF, ATTR_REL, ATTR_CHARSET, ATTR_TARGET, ATTR_HSEQ,
    ATTR_REFERER,
    ATTR_FRAMENAME, ATTR_TITLE, ATTR_ACCESSKEY, ATTR_CORE
};
enum HtmlTagAttributes ALST_P[] = { ATTR_ALIGN, ATTR_CORE };
enum HtmlTagAttributes ALST_UL[] = { ATTR_START, ATTR_TYPE, ATTR_CORE };
enum HtmlTagAttributes ALST_LI[] = { ATTR_TYPE, ATTR_VALUE, ATTR_CORE };
enum HtmlTagAttributes ALST_HR[] = { ATTR_WIDTH, ATTR_ALIGN, ATTR_CORE };
enum HtmlTagAttributes ALST_LINK[] = { ATTR_HREF, ATTR_HSEQ, ATTR_REL, ATTR_REV,
    ATTR_TITLE, ATTR_TYPE, ATTR_CORE };
enum HtmlTagAttributes ALST_DL[] = { ATTR_COMPACT, ATTR_CORE };
enum HtmlTagAttributes ALST_PRE[] = { ATTR_FOR_TABLE, ATTR_CORE };
enum HtmlTagAttributes ALST_IMG[] = { ATTR_SRC, ATTR_ALT, ATTR_WIDTH, ATTR_HEIGHT, ATTR_ALIGN, ATTR_USEMAP,
    ATTR_ISMAP, ATTR_TITLE, ATTR_PRE_INT, ATTR_CORE };
enum HtmlTagAttributes ALST_TABLE[] = { ATTR_BORDER, ATTR_WIDTH, ATTR_HBORDER, ATTR_CELLSPACING,
    ATTR_CELLPADDING, ATTR_VSPACE, ATTR_CORE };
enum HtmlTagAttributes ALST_DOCTYPE[] = { ATTR_PUBLIC }; /* only (html and) public should be checked */
enum HtmlTagAttributes ALST_META[] = { ATTR_HTTP_EQUIV, ATTR_CONTENT, ATTR_CHARSET, ATTR_CORE };
enum HtmlTagAttributes ALST_FRAME[] = { ATTR_SRC, ATTR_NAME, ATTR_CORE };
enum HtmlTagAttributes ALST_FRAMESET[] = { ATTR_COLS, ATTR_ROWS, ATTR_CORE };
enum HtmlTagAttributes ALST_NOFRAMES[] = { ATTR_CORE };
enum HtmlTagAttributes ALST_FORM[] = { ATTR_METHOD, ATTR_ACTION, ATTR_CHARSET, ATTR_ACCEPT_CHARSET,
    ATTR_ENCTYPE, ATTR_TARGET, ATTR_NAME, ATTR_CORE };
enum HtmlTagAttributes ALST_INPUT[] = { ATTR_TYPE, ATTR_VALUE, ATTR_NAME, ATTR_CHECKED, ATTR_ACCEPT, ATTR_SIZE,
    ATTR_MAXLENGTH, ATTR_ALT, ATTR_READONLY, ATTR_SRC, ATTR_WIDTH, ATTR_HEIGHT,
    ATTR_CORE };
enum HtmlTagAttributes ALST_BUTTON[] = { ATTR_TYPE, ATTR_VALUE, ATTR_NAME, ATTR_CORE };
enum HtmlTagAttributes ALST_TEXTAREA[] = { ATTR_COLS, ATTR_ROWS, ATTR_NAME, ATTR_READONLY, ATTR_CORE };
enum HtmlTagAttributes ALST_SELECT[] = { ATTR_NAME, ATTR_MULTIPLE, ATTR_CORE };
enum HtmlTagAttributes ALST_OPTION[] = { ATTR_VALUE, ATTR_LABEL, ATTR_SELECTED, ATTR_CORE };
enum HtmlTagAttributes ALST_ISINDEX[] = { ATTR_ACTION, ATTR_PROMPT, ATTR_CORE };
enum HtmlTagAttributes ALST_MAP[] = { ATTR_NAME, ATTR_CORE };
enum HtmlTagAttributes ALST_AREA[] = { ATTR_HREF, ATTR_TARGET, ATTR_ALT, ATTR_SHAPE, ATTR_COORDS, ATTR_CORE };
enum HtmlTagAttributes ALST_BASE[] = { ATTR_HREF, ATTR_TARGET, ATTR_CORE };
enum HtmlTagAttributes ALST_BODY[] = { ATTR_BACKGROUND, ATTR_CORE };
enum HtmlTagAttributes ALST_TR[] = { ATTR_ALIGN, ATTR_VALIGN, ATTR_CORE };
enum HtmlTagAttributes ALST_TD[] = { ATTR_COLSPAN, ATTR_ROWSPAN, ATTR_ALIGN, ATTR_VALIGN, ATTR_WIDTH,
    ATTR_NOWRAP, ATTR_CORE };
enum HtmlTagAttributes ALST_BGSOUND[] = { ATTR_SRC, ATTR_CORE };
enum HtmlTagAttributes ALST_APPLET[] = { ATTR_ARCHIVE, ATTR_CORE };
enum HtmlTagAttributes ALST_EMBED[] = { ATTR_SRC, ATTR_CORE };

enum HtmlTagAttributes ALST_TEXTAREA_INT[] = { ATTR_TEXTAREANUMBER };
enum HtmlTagAttributes ALST_SELECT_INT[] = { ATTR_SELECTNUMBER };
enum HtmlTagAttributes ALST_TABLE_ALT[] = { ATTR_TID };
enum HtmlTagAttributes ALST_SYMBOL[] = { ATTR_TYPE };
enum HtmlTagAttributes ALST_TITLE_ALT[] = { ATTR_TITLE };
enum HtmlTagAttributes ALST_FORM_INT[] = { ATTR_METHOD, ATTR_ACTION, ATTR_CHARSET, ATTR_ACCEPT_CHARSET,
    ATTR_ENCTYPE, ATTR_TARGET, ATTR_NAME, ATTR_FID };
enum HtmlTagAttributes ALST_INPUT_ALT[] = { ATTR_HSEQ, ATTR_FID, ATTR_NO_EFFECT, ATTR_TYPE, ATTR_NAME, ATTR_VALUE,
    ATTR_CHECKED, ATTR_ACCEPT, ATTR_SIZE, ATTR_MAXLENGTH, ATTR_READONLY,
    ATTR_TEXTAREANUMBER,
    ATTR_SELECTNUMBER, ATTR_ROWS, ATTR_TOP_MARGIN, ATTR_BOTTOM_MARGIN };
enum HtmlTagAttributes ALST_IMG_ALT[] = { ATTR_SRC, ATTR_WIDTH, ATTR_HEIGHT, ATTR_USEMAP, ATTR_ISMAP, ATTR_HSEQ,
    ATTR_XOFFSET, ATTR_YOFFSET, ATTR_TOP_MARGIN, ATTR_BOTTOM_MARGIN,
    ATTR_TITLE };
enum HtmlTagAttributes ALST_NOP[] = { ATTR_CORE };

struct TagInfo TagMAP[MAX_HTMLTAG] = {
    { NULL, NULL, 0, 0 }, /*   0 HTML_UNKNOWN    */
    { "a", ALST_A, ARR_SZ(ALST_A), 0 }, /*   1 HTML_A          */
    { "/a", NULL, 0, TFLG_END }, /*   2 HTML_N_A        */
    { "h", ALST_P, ARR_SZ(ALST_P), 0 }, /*   3 HTML_H          */
    { "/h", NULL, 0, TFLG_END }, /*   4 HTML_N_H        */
    { "p", ALST_P, ARR_SZ(ALST_P), 0 }, /*   5 HTML_P          */
    { "br", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*   6 HTML_BR         */
    { "b", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*   7 HTML_B          */
    { "/b", NULL, 0, TFLG_END }, /*   8 HTML_N_B        */
    { "ul", ALST_UL, ARR_SZ(ALST_UL), 0 }, /*   9 HTML_UL         */
    { "/ul", NULL, 0, TFLG_END }, /*  10 HTML_N_UL       */
    { "li", ALST_LI, ARR_SZ(ALST_LI), 0 }, /*  11 HTML_LI         */
    { "ol", ALST_UL, ARR_SZ(ALST_UL), 0 }, /*  12 HTML_OL         */
    { "/ol", NULL, 0, TFLG_END }, /*  13 HTML_N_OL       */
    { "title", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  14 HTML_TITLE      */
    { "/title", NULL, 0, TFLG_END }, /*  15 HTML_N_TITLE    */
    { "hr", ALST_HR, ARR_SZ(ALST_HR), 0 }, /*  16 HTML_HR         */
    { "dl", ALST_DL, ARR_SZ(ALST_DL), 0 }, /*  17 HTML_DL         */
    { "/dl", NULL, 0, TFLG_END }, /*  18 HTML_N_DL       */
    { "dt", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  19 HTML_DT         */
    { "dd", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  20 HTML_DD         */
    { "pre", ALST_PRE, ARR_SZ(ALST_PRE), 0 }, /*  21 HTML_PRE        */
    { "/pre", NULL, 0, TFLG_END }, /*  22 HTML_N_PRE      */
    { "blockquote", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  23 HTML_BLQ        */
    { "/blockquote", NULL, 0, TFLG_END }, /*  24 HTML_N_BLQ      */
    { "img", ALST_IMG, ARR_SZ(ALST_IMG), 0 }, /*  25 HTML_IMG        */
    { "listing", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  26 HTML_LISTING    */
    { "/listing", NULL, 0, TFLG_END }, /*  27 HTML_N_LISTING  */
    { "xmp", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  28 HTML_XMP        */
    { "/xmp", NULL, 0, TFLG_END }, /*  29 HTML_N_XMP      */
    { "plaintext", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  30 HTML_PLAINTEXT  */
    { "table", ALST_TABLE, ARR_SZ(ALST_TABLE), 0 }, /*  31 HTML_TABLE      */
    { "/table", NULL, 0, TFLG_END }, /*  32 HTML_N_TABLE    */
    { "meta", ALST_META, ARR_SZ(ALST_META), 0 }, /*  33 HTML_META       */
    { "/p", NULL, 0, TFLG_END }, /*  34 HTML_N_P        */
    { "frame", ALST_FRAME, ARR_SZ(ALST_FRAME), 0 }, /*  35 HTML_FRAME      */
    { "frameset", ALST_FRAMESET, ARR_SZ(ALST_FRAMESET), 0 }, /*  36 HTML_FRAMESET   */
    { "/frameset", NULL, 0, TFLG_END }, /*  37 HTML_N_FRAMESET */
    { "center", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  38 HTML_CENTER     */
    { "/center", NULL, 0, TFLG_END }, /*  39 HTML_N_CENTER   */
    { "font", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  40 HTML_FONT       */
    { "/font", NULL, 0, TFLG_END }, /*  41 HTML_N_FONT     */
    { "form", ALST_FORM, ARR_SZ(ALST_FORM), 0 }, /*  42 HTML_FORM       */
    { "/form", NULL, 0, TFLG_END }, /*  43 HTML_N_FORM     */
    { "input", ALST_INPUT, ARR_SZ(ALST_INPUT), 0 }, /*  44 HTML_INPUT      */
    { "textarea", ALST_TEXTAREA, ARR_SZ(ALST_TEXTAREA), 0 }, /*  45 HTML_TEXTAREA   */
    { "/textarea", NULL, 0, TFLG_END }, /*  46 HTML_N_TEXTAREA */
    { "select", ALST_SELECT, ARR_SZ(ALST_SELECT), 0 }, /*  47 HTML_SELECT     */
    { "/select", NULL, 0, TFLG_END }, /*  48 HTML_N_SELECT   */
    { "option", ALST_OPTION, ARR_SZ(ALST_OPTION), 0 }, /*  49 HTML_OPTION     */
    { "nobr", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  50 HTML_NOBR       */
    { "/nobr", NULL, 0, TFLG_END }, /*  51 HTML_N_NOBR     */
    { "div", ALST_P, ARR_SZ(ALST_P), 0 }, /*  52 HTML_DIV        */
    { "/div", NULL, 0, TFLG_END }, /*  53 HTML_N_DIV      */
    { "isindex", ALST_ISINDEX, ARR_SZ(ALST_ISINDEX), 0 }, /*  54 HTML_ISINDEX    */
    { "map", ALST_MAP, ARR_SZ(ALST_MAP), 0 }, /*  55 HTML_MAP        */
    { "/map", NULL, 0, TFLG_END }, /*  56 HTML_N_MAP      */
    { "area", ALST_AREA, ARR_SZ(ALST_AREA), 0 }, /*  57 HTML_AREA       */
    { "script", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  58 HTML_SCRIPT     */
    { "/script", NULL, 0, TFLG_END }, /*  59 HTML_N_SCRIPT   */
    { "base", ALST_BASE, ARR_SZ(ALST_BASE), 0 }, /*  60 HTML_BASE       */
    { "del", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  61 HTML_DEL        */
    { "/del", NULL, 0, TFLG_END }, /*  62 HTML_N_DEL      */
    { "ins", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  63 HTML_INS        */
    { "/ins", NULL, 0, TFLG_END }, /*  64 HTML_N_INS      */
    { "u", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  65 HTML_U          */
    { "/u", NULL, 0, TFLG_END }, /*  66 HTML_N_U        */
    { "style", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  67 HTML_STYLE      */
    { "/style", NULL, 0, TFLG_END }, /*  68 HTML_N_STYLE    */
    { "wbr", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  69 HTML_WBR        */
    { "em", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  70 HTML_EM         */
    { "/em", NULL, 0, TFLG_END }, /*  71 HTML_N_EM       */
    { "body", ALST_BODY, ARR_SZ(ALST_BODY), 0 }, /*  72 HTML_BODY       */
    { "/body", NULL, 0, TFLG_END }, /*  73 HTML_N_BODY     */
    { "tr", ALST_TR, ARR_SZ(ALST_TR), 0 }, /*  74 HTML_TR         */
    { "/tr", NULL, 0, TFLG_END }, /*  75 HTML_N_TR       */
    { "td", ALST_TD, ARR_SZ(ALST_TD), 0 }, /*  76 HTML_TD         */
    { "/td", NULL, 0, TFLG_END }, /*  77 HTML_N_TD       */
    { "caption", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  78 HTML_CAPTION    */
    { "/caption", NULL, 0, TFLG_END }, /*  79 HTML_N_CAPTION  */
    { "th", ALST_TD, ARR_SZ(ALST_TD), 0 }, /*  80 HTML_TH         */
    { "/th", NULL, 0, TFLG_END }, /*  81 HTML_N_TH       */
    { "thead", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  82 HTML_THEAD      */
    { "/thead", NULL, 0, TFLG_END }, /*  83 HTML_N_THEAD    */
    { "tbody", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  84 HTML_TBODY      */
    { "/tbody", NULL, 0, TFLG_END }, /*  85 HTML_N_TBODY    */
    { "tfoot", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  86 HTML_TFOOT      */
    { "/tfoot", NULL, 0, TFLG_END }, /*  87 HTML_N_TFOOT    */
    { "colgroup", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  88 HTML_COLGROUP   */
    { "/colgroup", NULL, 0, TFLG_END }, /*  89 HTML_N_COLGROUP */
    { "col", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  90 HTML_COL        */
    { "bgsound", ALST_BGSOUND, ARR_SZ(ALST_BGSOUND), 0 }, /*  91 HTML_BGSOUND    */
    { "applet", ALST_APPLET, ARR_SZ(ALST_APPLET), 0 }, /*  92 HTML_APPLET     */
    { "embed", ALST_EMBED, ARR_SZ(ALST_EMBED), 0 }, /*  93 HTML_EMBED      */
    { "/option", NULL, 0, TFLG_END }, /*  94 HTML_N_OPTION   */
    { "head", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /*  95 HTML_HEAD       */
    { "/head", NULL, 0, TFLG_END }, /*  96 HTML_N_HEAD     */
    { "doctype", ALST_DOCTYPE, ARR_SZ(ALST_DOCTYPE), 0 }, /*  97 HTML_DOCTYPE    */
    { "noframes", ALST_NOFRAMES, ARR_SZ(ALST_NOFRAMES), 0 }, /*  98 HTML_NOFRAMES   */
    { "/noframes", NULL, 0, TFLG_END }, /*  99 HTML_N_NOFRAMES */

    { "sup", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 100 HTML_SUP        */
    { "/sup", NULL, 0, TFLG_END }, /* 101 HTML_N_SUP      */
    { "sub", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 102 HTML_SUB        */
    { "/sub", NULL, 0, TFLG_END }, /* 103 HTML_N_SUB      */
    { "link", ALST_LINK, ARR_SZ(ALST_LINK), 0 }, /* 104 HTML_LINK       */
    { "s", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 105 HTML_S          */
    { "/s", NULL, 0, TFLG_END }, /* 106 HTML_N_S        */
    { "q", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 107 HTML_Q          */
    { "/q", NULL, 0, TFLG_END }, /* 108 HTML_N_Q        */
    { "i", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 109 HTML_I          */
    { "/i", NULL, 0, TFLG_END }, /* 110 HTML_N_I        */
    { "strong", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 111 HTML_STRONG     */
    { "/strong", NULL, 0, TFLG_END }, /* 112 HTML_N_STRONG   */
    { "span", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 113 HTML_SPAN       */
    { "/span", NULL, 0, TFLG_END }, /* 114 HTML_N_SPAN     */
    { "abbr", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 115 HTML_ABBR       */
    { "/abbr", NULL, 0, TFLG_END }, /* 116 HTML_N_ABBR     */
    { "acronym", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 117 HTML_ACRONYM    */
    { "/acronym", NULL, 0, TFLG_END }, /* 118 HTML_N_ACRONYM  */
    { "basefont", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 119 HTML_BASEFONT   */
    { "bdo", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 120 HTML_BDO        */
    { "/bdo", NULL, 0, TFLG_END }, /* 121 HTML_N_BDO      */
    { "big", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 122 HTML_BIG        */
    { "/big", NULL, 0, TFLG_END }, /* 123 HTML_N_BIG      */
    { "button", ALST_BUTTON, ARR_SZ(ALST_BUTTON), 0 }, /* 124 HTML_BUTTON     */
    { "/button", NULL, 0, TFLG_END }, /* 125 HTML_N_BUTTON   */
    { "fieldset", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 126 HTML_FIELDSET   */
    { "/fieldset", NULL, 0, TFLG_END }, /* 127 HTML_N_FIELDSET */
    { "iframe", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 128 HTML_IFRAME     */
    { "label", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 129 HTML_LABEL      */
    { "/label", NULL, 0, TFLG_END }, /* 130 HTML_N_LABEL    */
    { "legend", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 131 HTML_LEGEND     */
    { "/legend", NULL, 0, TFLG_END }, /* 132 HTML_N_LEGEND   */
    { "noscript", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 133 HTML_NOSCRIPT   */
    { "/noscript", NULL, 0, TFLG_END }, /* 134 HTML_N_NOSCRIPT */
    { "object", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 135 HTML_OBJECT     */
    { "optgroup", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 136 HTML_OPTGROUP   */
    { "/optgroup", NULL, 0, TFLG_END }, /* 137 HTML_N_OPTGROUP */
    { "param", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 138 HTML_PARAM      */
    { "small", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 139 HTML_SMALL      */
    { "/small", NULL, 0, TFLG_END }, /* 140 HTML_N_SMALL    */
    { "figure", ALST_P, ARR_SZ(ALST_P), 0 }, /* 141 HTML_FIGURE     */
    { "/figure", NULL, 0, TFLG_END }, /* 142 HTML_N_FIGURE   */
    { "figcaption", ALST_P, ARR_SZ(ALST_P), 0 }, /* 143 HTML_FIGCAPTION */
    { "/figcaption", NULL, 0, TFLG_END }, /* 144 HTML_N_FIGCAPTION */
    { "section", ALST_NOP, ARR_SZ(ALST_NOP), 0 }, /* 145 HTML_SECTION    */
    { "/section", NULL, 0, TFLG_END }, /* 146 HTML_N_SECTION  */
    { "/dt", NULL, 0, TFLG_END }, /* 147 HTML_N_DT       */
    { "/dd", NULL, 0, TFLG_END }, /* 148 HTML_N_DD       */
    { "dfn", ALST_ID, ARR_SZ(ALST_ID), 0 }, /* 149 HTML_DFN */
    { "/dfn", NULL, 0, TFLG_END }, /* 150 HTML_N_DFN */

    { NULL, NULL, 0, 0 }, /* 151 Undefined */
    { NULL, NULL, 0, 0 }, /* 152 Undefined */
    { NULL, NULL, 0, 0 }, /* 153 Undefined */
    { NULL, NULL, 0, 0 }, /* 154 Undefined */
    { NULL, NULL, 0, 0 }, /* 155 Undefined */
    { NULL, NULL, 0, 0 }, /* 156 Undefined */
    { NULL, NULL, 0, 0 }, /* 157 Undefined */
    { NULL, NULL, 0, 0 }, /* 158 Undefined */
    { NULL, NULL, 0, 0 }, /* 159 Undefined */

    /* pseudo tag */
    { "select_int", ALST_SELECT_INT, ARR_SZ(ALST_SELECT_INT), TFLG_INT }, /* 160 HTML_SELECT_INT     */
    { "/select_int", NULL, 0, TFLG_INT | TFLG_END }, /* 161 HTML_N_SELECT_INT   */
    { "option_int", ALST_OPTION, ARR_SZ(ALST_OPTION), TFLG_INT }, /* 162 HTML_OPTION_INT     */
    { "textarea_int", ALST_TEXTAREA_INT, ARR_SZ(ALST_TEXTAREA_INT), TFLG_INT }, /* 163 HTML_TEXTAREA_INT   */
    { "/textarea_int", NULL, 0, TFLG_INT | TFLG_END }, /* 164 HTML_N_TEXTAREA_INT */
    { "table_alt", ALST_TABLE_ALT, ARR_SZ(ALST_TABLE_ALT), TFLG_INT }, /* 165 HTML_TABLE_ALT      */
    { "symbol", ALST_SYMBOL, ARR_SZ(ALST_SYMBOL), TFLG_INT }, /* 166 HTML_SYMBOL         */
    { "/symbol", NULL, 0, TFLG_INT | TFLG_END }, /* 167 HTML_N_SYMBOL       */
    { "pre_int", NULL, 0, TFLG_INT }, /* 168 HTML_PRE_INT        */
    { "/pre_int", NULL, 0, TFLG_INT | TFLG_END }, /* 169 HTML_N_PRE_INT      */
    { "title_alt", ALST_TITLE_ALT, ARR_SZ(ALST_TITLE_ALT), TFLG_INT }, /* 170 HTML_TITLE_ALT      */
    { "form_int", ALST_FORM_INT, ARR_SZ(ALST_FORM_INT), TFLG_INT }, /* 171 HTML_FORM_INT       */
    { "/form_int", NULL, 0, TFLG_INT | TFLG_END }, /* 172 HTML_N_FORM_INT     */
    { "dl_compact", NULL, 0, TFLG_INT }, /* 173 HTML_DL_COMPACT     */
    { "input_alt", ALST_INPUT_ALT, ARR_SZ(ALST_INPUT_ALT), TFLG_INT }, /* 174 HTML_INPUT_ALT      */
    { "/input_alt", NULL, 0, TFLG_INT | TFLG_END }, /* 175 HTML_N_INPUT_ALT    */
    { "img_alt", ALST_IMG_ALT, ARR_SZ(ALST_IMG_ALT), TFLG_INT }, /* 176 HTML_IMG_ALT        */
    { "/img_alt", NULL, 0, TFLG_INT | TFLG_END }, /* 177 HTML_N_IMG_ALT      */
    { " ", ALST_NOP, ARR_SZ(ALST_NOP), TFLG_INT }, /* 178 HTML_NOP            */
    { "pre_plain", NULL, 0, TFLG_INT }, /* 179 HTML_PRE_PLAIN      */
    { "/pre_plain", NULL, 0, TFLG_INT | TFLG_END }, /* 180 HTML_N_PRE_PLAIN    */
    { "internal", NULL, 0, TFLG_INT }, /* 181 HTML_INTERNAL       */
    { "/internal", NULL, 0, TFLG_INT | TFLG_END }, /* 182 HTML_N_INTERNAL     */
    { "div_int", ALST_P, ARR_SZ(ALST_P), TFLG_INT }, /* 183 HTML_DIV_INT        */
    { "/div_int", NULL, 0, TFLG_INT | TFLG_END }, /* 184 HTML_N_DIV_INT      */
};

#define MAX_CMD_LEN 128

enum HtmlTags gethtmlcmd(const char** s)
{
    extern Hash_si tagtable;
    char cmdstr[MAX_CMD_LEN];
    char* p = cmdstr;
    const char* save = *s;
    int cmd;

    (*s)++;
    /* first character */
    if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
        *(p++) = TOLOWER(**s);
        (*s)++;
    } else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        SKIP_BLANKS(s);
    while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN) {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    if (p - cmdstr == MAX_CMD_LEN) {
        /* buffer overflow: perhaps caused by bad HTML source */
        *s = save + 1;
        return HTML_UNKNOWN;
    }
    *p = '\0';

    /* hash search */
    cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
    while (**s && **s != '>')
        (*s)++;
    if (**s == '>')
        (*s)++;
    return cmd;
}
