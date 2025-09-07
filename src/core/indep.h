#pragma once
#include <alloc.h>
#include <Str.h>
#include <stdbool.h>

extern unsigned char QUOTE_MAP[];
extern char* HTML_QUOTE_MAP[];
#define HTML_QUOTE_MASK 0x07 /* &, <, >, ", ' */
#define SHELL_UNSAFE_MASK 0x08 /* [^A-Za-z0-9_./:\200-\377] */
#define URL_QUOTE_MASK 0x10 /* [\0- \177-\377] */
#define FILE_QUOTE_MASK 0x30 /* [\0- #%&+:?\177-\377] */
#define URL_UNSAFE_MASK 0x70 /* [^A-Za-z0-9_$\-.] */
#define GET_QUOTE_TYPE(c) QUOTE_MAP[(int)(unsigned char)(c)]
#define is_html_quote(c) (GET_QUOTE_TYPE(c) & HTML_QUOTE_MASK)
#define is_shell_unsafe(c) (GET_QUOTE_TYPE(c) & SHELL_UNSAFE_MASK)
#define is_url_quote(c) (GET_QUOTE_TYPE(c) & URL_QUOTE_MASK)
#define is_file_quote(c) (GET_QUOTE_TYPE(c) & FILE_QUOTE_MASK)
#define is_url_unsafe(c) (GET_QUOTE_TYPE(c) & URL_UNSAFE_MASK)
#define html_quote_char(c) HTML_QUOTE_MAP[(int)is_html_quote(c)]

extern long long strtoclen(const char* s);
extern char* conv_entity(unsigned int ch);
extern int getescapechar(char** s);
extern char* getescapecmd(char** s);
extern int strCmp(const void* s1, const void* s2);
extern char* currentdir(void);
extern const char* expandPath(const char* name);
extern int strcasemstr(char* str, char* srch[], char** ret_ptr);
int strmatchlen(const char* s1, const char* s2, int maxlen);
extern const char* remove_space(const char* str);
extern const char* html_quote(const char* str);
extern char* html_unquote(char* str);
extern char* file_quote(char* str);
extern const char* url_quote(const char* str);
extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
extern char* shell_quote(const char* str);

