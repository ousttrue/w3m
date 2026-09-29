/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_INDEP_H
#define W3M_INDEP_H

#include "Str.h"
#include "config.h"
#include "libwc/wc.h"
#include "charset.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) > (b) ? (b) : (a))

#define FALSE 0
#define TRUE 1

enum {
    RAW_MODE,
    PAGER_MODE,
    HTML_MODE,
    HEADER_MODE,
};

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

extern size_t strtoclen(const char* s);
extern char* conv_entity(unsigned int ch);
extern char* getescapestr(char** s, int is_attr, int* pis_simple);
extern char* getescapecmd(char** s);
extern char* allocStr(const char* s, int len);
extern int strCmp(const void* s1, const void* s2);
extern char* currentdir(void);
#ifndef HAVE_STRCASESTR
extern char* strcasestr(const char* s1, const char* s2);
#endif
int strmatchlen(const char* s1, const char* s2, int maxlen);
extern char* remove_space(char* str);
extern int non_null(const char* s);
extern void cleanup_line(Str s, int mode);
extern char* html_quote(char* str);
extern char* html_unquote(char* str);
extern char* html_unquote_attr(char* str);
extern char* file_quote(char* str);
extern char* file_unquote(char* str);
extern char* url_quote(char* str);
static inline char* url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict(x, InnerCharset, c)->ptr);
}

extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), TRUE, FALSE)
extern const char* shell_quote(const char* str);

extern char* CurrentDir;
extern int CurrentPid;

#endif
