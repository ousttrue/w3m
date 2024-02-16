#include "hash.h"
#include <stdio.h>
#include "html_command.h"

static HashItem_si MyHashItem[] = {
    /* 0 */ {"option_int", HTML_OPTION_INT, &MyHashItem[1]},
    /* 1 */ {"figcaption", HTML_FIGCAPTION, &MyHashItem[2]},
    /* 2 */ {"/form_int", HTML_N_FORM_INT, &MyHashItem[3]},
    /* 3 */ {"/kbd", HTML_NOP, &MyHashItem[4]},
    /* 4 */ {"dd", HTML_DD, &MyHashItem[5]},
    /* 5 */ {"/dir", HTML_N_UL, NULL},
    /* 6 */ {"/body", HTML_N_BODY, &MyHashItem[7]},
    /* 7 */ {"noframes", HTML_NOFRAMES, NULL},
    /* 8 */ {"bdo", HTML_BDO, &MyHashItem[9]},
    /* 9 */ {"base", HTML_BASE, NULL},
    /* 10 */ {"/dt", HTML_N_DT, &MyHashItem[11]},
    /* 11 */ {"/div", HTML_N_DIV, NULL},
    /* 12 */ {"big", HTML_BIG, &MyHashItem[13]},
    /* 13 */ {"tbody", HTML_TBODY, &MyHashItem[14]},
    /* 14 */ {"meta", HTML_META, &MyHashItem[15]},
    /* 15 */ {"i", HTML_I, NULL},
    /* 16 */ {"label", HTML_LABEL, &MyHashItem[17]},
    /* 17 */ {"/_symbol", HTML_N_SYMBOL, &MyHashItem[18]},
    /* 18 */ {"sup", HTML_SUP, &MyHashItem[19]},
    /* 19 */ {"/p", HTML_N_P, NULL},
    /* 20 */ {"/q", HTML_N_Q, NULL},
    /* 21 */ {"input_alt", HTML_INPUT_ALT, &MyHashItem[22]},
    /* 22 */ {"dl", HTML_DL, NULL},
    /* 23 */ {"/tbody", HTML_N_TBODY, &MyHashItem[24]},
    /* 24 */ {"/s", HTML_N_S, NULL},
    /* 25 */ {"/label", HTML_N_LABEL, &MyHashItem[26]},
    /* 26 */ {"del", HTML_DEL, &MyHashItem[27]},
    /* 27 */ {"xmp", HTML_XMP, &MyHashItem[28]},
    /* 28 */ {"br", HTML_BR, NULL},
    /* 29 */ {"iframe", HTML_IFRAME, &MyHashItem[30]},
    /* 30 */ {"link", HTML_LINK, &MyHashItem[31]},
    /* 31 */ {"/u", HTML_N_U, &MyHashItem[32]},
    /* 32 */ {"em", HTML_EM, NULL},
    /* 33 */ {"title_alt", HTML_TITLE_ALT, &MyHashItem[34]},
    /* 34 */ {"caption", HTML_CAPTION, &MyHashItem[35]},
    /* 35 */ {"plaintext", HTML_PLAINTEXT, &MyHashItem[36]},
    /* 36 */ {"p", HTML_P, NULL},
    /* 37 */ {"q", HTML_Q, &MyHashItem[38]},
    /* 38 */ {"blockquote", HTML_BLQ, &MyHashItem[39]},
    /* 39 */ {"menu", HTML_UL, NULL},
    /* 40 */ {"fieldset", HTML_FIELDSET, &MyHashItem[41]},
    /* 41 */ {"/colgroup", HTML_N_COLGROUP, &MyHashItem[42]},
    /* 42 */ {"dfn", HTML_DFN, NULL},
    /* 43 */ {"s", HTML_S, &MyHashItem[44]},
    /* 44 */ {"strong", HTML_STRONG, NULL},
    /* 45 */ {"dt", HTML_DT, NULL},
    /* 46 */ {"u", HTML_U, NULL},
    /* 47 */ {"/map", HTML_N_MAP, &MyHashItem[48]},
    /* 48 */ {"/frameset", HTML_N_FRAMESET, &MyHashItem[49]},
    /* 49 */ {"/ol", HTML_N_OL, NULL},
    /* 50 */ {"noscript", HTML_NOSCRIPT, &MyHashItem[51]},
    /* 51 */ {"legend", HTML_LEGEND, &MyHashItem[52]},
    /* 52 */ {"/td", HTML_N_TD, NULL},
    /* 53 */ {"li", HTML_LI, NULL},
    /* 54 */ {"html", HTML_BODY, &MyHashItem[55]},
    /* 55 */ {"hr", HTML_HR, NULL},
    /* 56 */ {"/strong", HTML_N_STRONG, NULL},
    /* 57 */ {"small", HTML_SMALL, &MyHashItem[58]},
    /* 58 */ {"/th", HTML_N_TH, &MyHashItem[59]},
    /* 59 */ {"option", HTML_OPTION, &MyHashItem[60]},
    /* 60 */ {"/span", HTML_N_SPAN, &MyHashItem[61]},
    /* 61 */ {"kbd", HTML_NOP, &MyHashItem[62]},
    /* 62 */ {"dir", HTML_UL, NULL},
    /* 63 */ {"col", HTML_COL, NULL},
    /* 64 */ {"param", HTML_PARAM, NULL},
    /* 65 */ {"/figcaption", HTML_N_FIGCAPTION, &MyHashItem[66]},
    /* 66 */ {"/small", HTML_N_SMALL, &MyHashItem[67]},
    /* 67 */ {"/legend", HTML_N_LEGEND, &MyHashItem[68]},
    /* 68 */ {"/caption", HTML_N_CAPTION, &MyHashItem[69]},
    /* 69 */ {"div", HTML_DIV, NULL},
    /* 70 */ {"/abbr", HTML_N_ABBR, &MyHashItem[71]},
    /* 71 */ {"head", HTML_HEAD, &MyHashItem[72]},
    /* 72 */ {"ol", HTML_OL, &MyHashItem[73]},
    /* 73 */ {"/ul", HTML_N_UL, NULL},
    /* 74 */ {"/ins", HTML_N_INS, &MyHashItem[75]},
    /* 75 */ {"area", HTML_AREA, NULL},
    /* 76 */ {"pre_plain", HTML_PRE_PLAIN, &MyHashItem[77]},
    /* 77 */ {"button", HTML_BUTTON, &MyHashItem[78]},
    /* 78 */ {"td", HTML_TD, &MyHashItem[79]},
    /* 79 */ {"/option", HTML_N_OPTION, NULL},
    /* 80 */ {"/noframes", HTML_N_NOFRAMES, NULL},
    /* 81 */ {"/tr", HTML_N_TR, &MyHashItem[82]},
    /* 82 */ {"nobr", HTML_NOBR, NULL},
    /* 83 */ {"img_alt", HTML_IMG_ALT, &MyHashItem[84]},
    /* 84 */ {"table_alt", HTML_TABLE_ALT, &MyHashItem[85]},
    /* 85 */ {"th", HTML_TH, &MyHashItem[86]},
    /* 86 */ {"script", HTML_SCRIPT, &MyHashItem[87]},
    /* 87 */ {"/tt", HTML_NOP, NULL},
    /* 88 */ {"code", HTML_NOP, NULL},
    /* 89 */ {"optgroup", HTML_OPTGROUP, &MyHashItem[90]},
    /* 90 */ {"samp", HTML_NOP, NULL},
    /* 91 */ {"textarea", HTML_TEXTAREA, NULL},
    /* 92 */ {"textarea_int", HTML_TEXTAREA_INT, &MyHashItem[93]},
    /* 93 */ {"/button", HTML_N_BUTTON, NULL},
    /* 94 */ {"table", HTML_TABLE, &MyHashItem[95]},
    /* 95 */ {"img", HTML_IMG, &MyHashItem[96]},
    /* 96 */ {"/blockquote", HTML_N_BLQ, NULL},
    /* 97 */ {"applet", HTML_APPLET, &MyHashItem[98]},
    /* 98 */ {"map", HTML_MAP, &MyHashItem[99]},
    /* 99 */ {"ul", HTML_UL, NULL},
    /* 100 */ {"basefont", HTML_BASEFONT, &MyHashItem[101]},
    /* 101 */ {"/script", HTML_N_SCRIPT, &MyHashItem[102]},
    /* 102 */ {"center", HTML_CENTER, NULL},
    /* 103 */ {"/table", HTML_N_TABLE, &MyHashItem[104]},
    /* 104 */ {"cite", HTML_NOP, &MyHashItem[105]},
    /* 105 */ {"/h1", HTML_N_H, NULL},
    /* 106 */ {"/fieldset", HTML_N_FIELDSET, &MyHashItem[107]},
    /* 107 */ {"tr", HTML_TR, &MyHashItem[108]},
    /* 108 */ {"/h2", HTML_N_H, NULL},
    /* 109 */ {"image", HTML_IMG, &MyHashItem[110]},
    /* 110 */ {"/h3", HTML_N_H, NULL},
    /* 111 */ {"pre_int", HTML_PRE_INT, &MyHashItem[112]},
    /* 112 */ {"/font", HTML_N_FONT, &MyHashItem[113]},
    /* 113 */ {"tt", HTML_NOP, &MyHashItem[114]},
    /* 114 */ {"/h4", HTML_N_H, NULL},
    /* 115 */ {"body", HTML_BODY, &MyHashItem[116]},
    /* 116 */ {"/form", HTML_N_FORM, &MyHashItem[117]},
    /* 117 */ {"/h5", HTML_N_H, NULL},
    /* 118 */ {"/h6", HTML_N_H, NULL},
    /* 119 */ {"frame", HTML_FRAME, NULL},
    /* 120 */ {"/textarea_int", HTML_N_TEXTAREA_INT, &MyHashItem[121]},
    /* 121 */ {"/noscript", HTML_N_NOSCRIPT, &MyHashItem[122]},
    /* 122 */ {"/img_alt", HTML_N_IMG_ALT, &MyHashItem[123]},
    /* 123 */ {"/center", HTML_N_CENTER, NULL},
    /* 124 */ {"/pre", HTML_N_PRE, NULL},
    /* 125 */ {"tfoot", HTML_TFOOT, NULL},
    /* 126 */ {"ins", HTML_INS, NULL},
    /* 127 */ {"section", HTML_SECTION, &MyHashItem[128]},
    /* 128 */ {"/var", HTML_NOP, NULL},
    /* 129 */ {"h1", HTML_H, NULL},
    /* 130 */ {"/tfoot", HTML_N_TFOOT, &MyHashItem[131]},
    /* 131 */ {"input", HTML_INPUT, &MyHashItem[132]},
    /* 132 */ {"h2", HTML_H, NULL},
    /* 133 */ {"h3", HTML_H, NULL},
    /* 134 */ {"h4", HTML_H, NULL},
    /* 135 */ {"h5", HTML_H, NULL},
    /* 136 */ {"internal", HTML_INTERNAL, &MyHashItem[137]},
    /* 137 */ {"h6", HTML_H, NULL},
    /* 138 */ {"div_int", HTML_DIV_INT, &MyHashItem[139]},
    /* 139 */ {"select_int", HTML_SELECT_INT, &MyHashItem[140]},
    /* 140 */ {"/pre_int", HTML_N_PRE_INT, NULL},
    /* 141 */ {"figure", HTML_FIGURE, &MyHashItem[142]},
    /* 142 */ {"/menu", HTML_N_UL, NULL},
    /* 143 */ {"form_int", HTML_FORM_INT, &MyHashItem[144]},
    /* 144 */ {"/sub", HTML_N_SUB, NULL},
    /* 145 */ {"style", HTML_STYLE, &MyHashItem[146]},
    /* 146 */ {"address", HTML_BR, NULL},
    /* 147 */ {"/optgroup", HTML_N_OPTGROUP, NULL},
    /* 148 */ {"/textarea", HTML_N_TEXTAREA, NULL},
    /* 149 */ {"/section", HTML_N_SECTION, &MyHashItem[150]},
    /* 150 */ {"/input_alt", HTML_N_INPUT_ALT, &MyHashItem[151]},
    /* 151 */ {"span", HTML_SPAN, NULL},
    /* 152 */ {"/figure", HTML_N_FIGURE, &MyHashItem[153]},
    /* 153 */ {"doctype", HTML_DOCTYPE, &MyHashItem[154]},
    /* 154 */ {"/style", HTML_N_STYLE, NULL},
    /* 155 */ {"/html", HTML_N_BODY, NULL},
    /* 156 */ {"pre", HTML_PRE, &MyHashItem[157]},
    /* 157 */ {"title", HTML_TITLE, NULL},
    /* 158 */ {"abbr", HTML_ABBR, &MyHashItem[159]},
    /* 159 */ {"select", HTML_SELECT, NULL},
    /* 160 */ {"/bdo", HTML_N_BDO, &MyHashItem[161]},
    /* 161 */ {"acronym", HTML_ACRONYM, NULL},
    /* 162 */ {"/div_int", HTML_N_DIV_INT, &MyHashItem[163]},
    /* 163 */ {"var", HTML_NOP, NULL},
    /* 164 */ {"/big", HTML_N_BIG, &MyHashItem[165]},
    /* 165 */ {"/title", HTML_N_TITLE, NULL},
    /* 166 */ {"embed", HTML_EMBED, &MyHashItem[167]},
    /* 167 */ {"/sup", HTML_N_SUP, &MyHashItem[168]},
    /* 168 */ {"colgroup", HTML_COLGROUP, &MyHashItem[169]},
    /* 169 */ {"/head", HTML_N_HEAD, &MyHashItem[170]},
    /* 170 */ {"isindex", HTML_ISINDEX, NULL},
    /* 171 */ {"strike", HTML_S, &MyHashItem[172]},
    /* 172 */ {"listing", HTML_LISTING, NULL},
    /* 173 */ {"bgsound", HTML_BGSOUND, NULL},
    /* 174 */ {"/address", HTML_BR, NULL},
    /* 175 */ {"object", HTML_OBJECT, &MyHashItem[176]},
    /* 176 */ {"thead", HTML_THEAD, &MyHashItem[177]},
    /* 177 */ {"wbr", HTML_WBR, &MyHashItem[178]},
    /* 178 */ {"/del", HTML_N_DEL, &MyHashItem[179]},
    /* 179 */ {"/nobr", HTML_N_NOBR, &MyHashItem[180]},
    /* 180 */ {"/select", HTML_N_SELECT, &MyHashItem[181]},
    /* 181 */ {"frameset", HTML_FRAMESET, &MyHashItem[182]},
    /* 182 */ {"/xmp", HTML_N_XMP, NULL},
    /* 183 */ {"/dd", HTML_N_DD, NULL},
    /* 184 */ {"/code", HTML_NOP, NULL},
    /* 185 */ {"_symbol", HTML_SYMBOL, &MyHashItem[186]},
    /* 186 */ {"/thead", HTML_N_THEAD, &MyHashItem[187]},
    /* 187 */ {"/samp", HTML_NOP, &MyHashItem[188]},
    /* 188 */ {"/dfn", HTML_N_DFN, &MyHashItem[189]},
    /* 189 */ {"_id", HTML_NOP, NULL},
    /* 190 */ {"/strike", HTML_N_S, &MyHashItem[191]},
    /* 191 */ {"/a", HTML_N_A, NULL},
    /* 192 */ {"/select_int", HTML_N_SELECT_INT, &MyHashItem[193]},
    /* 193 */ {"sub", HTML_SUB, &MyHashItem[194]},
    /* 194 */ {"/b", HTML_N_B, NULL},
    /* 195 */ {"/internal", HTML_N_INTERNAL, NULL},
    /* 196 */ {"/acronym", HTML_N_ACRONYM, NULL},
    /* 197 */ {"/pre_plain", HTML_N_PRE_PLAIN, &MyHashItem[198]},
    /* 198 */ {"font", HTML_FONT, &MyHashItem[199]},
    /* 199 */ {"/dl", HTML_N_DL, NULL},
    /* 200 */ {"form", HTML_FORM, &MyHashItem[201]},
    /* 201 */ {"/cite", HTML_NOP, &MyHashItem[202]},
    /* 202 */ {"a", HTML_A, NULL},
    /* 203 */ {"b", HTML_B, NULL},
    /* 204 */ {"/listing", HTML_N_LISTING, &MyHashItem[205]},
    /* 205 */ {"/em", HTML_N_EM, &MyHashItem[206]},
    /* 206 */ {"/i", HTML_N_I, NULL},
};

static HashItem_si *MyHashItemTbl[] = {
    &MyHashItem[0],
    &MyHashItem[6],
    NULL,
    &MyHashItem[8],
    &MyHashItem[10],
    &MyHashItem[12],
    &MyHashItem[16],
    &MyHashItem[20],
    &MyHashItem[21],
    &MyHashItem[23],
    &MyHashItem[25],
    &MyHashItem[29],
    &MyHashItem[33],
    &MyHashItem[37],
    &MyHashItem[40],
    &MyHashItem[43],
    &MyHashItem[45],
    &MyHashItem[46],
    &MyHashItem[47],
    NULL,
    &MyHashItem[50],
    &MyHashItem[53],
    &MyHashItem[54],
    &MyHashItem[56],
    &MyHashItem[57],
    NULL,
    &MyHashItem[63],
    &MyHashItem[64],
    &MyHashItem[65],
    NULL,
    &MyHashItem[70],
    &MyHashItem[74],
    &MyHashItem[76],
    &MyHashItem[80],
    &MyHashItem[81],
    NULL,
    &MyHashItem[83],
    &MyHashItem[88],
    &MyHashItem[89],
    &MyHashItem[91],
    &MyHashItem[92],
    &MyHashItem[94],
    &MyHashItem[97],
    NULL,
    &MyHashItem[100],
    &MyHashItem[103],
    &MyHashItem[106],
    &MyHashItem[109],
    &MyHashItem[111],
    &MyHashItem[115],
    &MyHashItem[118],
    &MyHashItem[119],
    &MyHashItem[120],
    &MyHashItem[124],
    &MyHashItem[125],
    &MyHashItem[126],
    &MyHashItem[127],
    &MyHashItem[129],
    &MyHashItem[130],
    &MyHashItem[133],
    &MyHashItem[134],
    &MyHashItem[135],
    &MyHashItem[136],
    NULL,
    &MyHashItem[138],
    &MyHashItem[141],
    NULL,
    NULL,
    &MyHashItem[143],
    &MyHashItem[145],
    &MyHashItem[147],
    &MyHashItem[148],
    &MyHashItem[149],
    &MyHashItem[152],
    &MyHashItem[155],
    NULL,
    NULL,
    &MyHashItem[156],
    &MyHashItem[158],
    &MyHashItem[160],
    &MyHashItem[162],
    &MyHashItem[164],
    &MyHashItem[166],
    &MyHashItem[171],
    &MyHashItem[173],
    &MyHashItem[174],
    &MyHashItem[175],
    NULL,
    &MyHashItem[183],
    &MyHashItem[184],
    &MyHashItem[185],
    &MyHashItem[190],
    &MyHashItem[192],
    NULL,
    &MyHashItem[195],
    &MyHashItem[196],
    &MyHashItem[197],
    &MyHashItem[200],
    &MyHashItem[203],
    &MyHashItem[204],
};

Hash_si tagtable = { 100, MyHashItemTbl };
