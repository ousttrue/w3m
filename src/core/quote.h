#pragma once
#include <Str.h>

enum QuoteMask {
    HTML_QUOTE_MASK = 0x07 /* &, <, >, ", ' */,
    SHELL_UNSAFE_MASK = 0x08 /* [^A-Za-z0-9_./:\200-\377] */,
    URL_QUOTE_MASK = 0x10 /* [\0- \177-\377] */,
    FILE_QUOTE_MASK = 0x30 /* [\0- #%&+:?\177-\377] */,
    URL_UNSAFE_MASK = 0x70 /* [^A-Za-z0-9_$\-.] */,
};

enum QuoteMask GET_QUOTE_TYPE(unsigned char c);
inline static bool is_html_quote(unsigned char c) { return (GET_QUOTE_TYPE(c) & HTML_QUOTE_MASK); }
inline static bool is_shell_unsafe(unsigned char c) { return (GET_QUOTE_TYPE(c) & SHELL_UNSAFE_MASK); }
inline static bool is_url_quote(unsigned char c) { return (GET_QUOTE_TYPE(c) & URL_QUOTE_MASK); }
inline static bool is_file_quote(unsigned char c) { return (GET_QUOTE_TYPE(c) & FILE_QUOTE_MASK); }
inline static bool is_url_unsafe(unsigned char c) { return (GET_QUOTE_TYPE(c) & URL_UNSAFE_MASK); }

const char* remove_space(const char* str);

char* file_quote(char* str);
const char* url_quote(const char* str);
Str Str_url_unquote(Str x, int is_form, int safe);
Str Str_form_quote(Str x);
inline static Str Str_form_unquote(Str x) { return Str_url_unquote((x), true, false); }
extern char* shell_quote(const char* str);

const char* getWord(const char** str);
const char* getQWord(const char** str);

struct regex;
const char* getRegexWord(const char** str, struct regex** regex_ret);

const char* file_unquote(const char* str);
