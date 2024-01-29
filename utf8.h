#pragma once
#include "line.h"
#include <string>

#ifdef __clang__
typedef unsigned char char8_t;
#endif

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
