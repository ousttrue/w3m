#pragma once
#include <string>

typedef unsigned short Lineprop;

/*
 * Line Property
 */

#define P_CHARTYPE 0x3f00
#define PC_ASCII 0x0000
#define PC_CTRL 0x0100
#define PC_SYMBOL 0x8000

/* Effect ( standout/underline ) */
#define P_EFFECT 0x40ff
#define PE_NORMAL 0x00
#define PE_MARK 0x01
#define PE_UNDER 0x02
#define PE_STAND 0x04
#define PE_BOLD 0x08
#define PE_ANCHOR 0x10
#define PE_EMPH 0x08
#define PE_IMAGE 0x20
#define PE_FORM 0x40
#define PE_ACTIVE 0x80
#define PE_VISITED 0x4000

/* Extra effect */
#define PE_EX_ITALIC 0x01
#define PE_EX_INSERT 0x02
#define PE_EX_STRIKE 0x04

#define PE_EX_ITALIC_E PE_UNDER
#define PE_EX_INSERT_E PE_UNDER
#define PE_EX_STRIKE_E PE_STAND

#define CharType(c) ((c)&P_CHARTYPE)
#define CharEffect(c) ((c) & (P_EFFECT | PC_SYMBOL))
#define SetCharType(v, c) ((v) = (((v) & ~P_CHARTYPE) | (c)))

std::string codepoint_to_utf8(char32_t codepoint);

Lineprop get_mctype(unsigned char c);

// 1文字のbytelength
int get_mclen(const char8_t *c);

// 1文字のculmun幅
int get_mcwidth(const char8_t *c);

// 文字列のcolumn幅
int get_strwidth(const char8_t *c);

// 文字列のcolumn幅
int get_Str_strwidth(struct Str *c);
