#pragma once
#include "config.h"
#include <stddef.h>

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */
#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

#define RAW_MODE 0
#define PAGER_MODE 1
#define HTML_MODE 2
#define HEADER_MODE 3

extern unsigned char QUOTE_MAP[];
extern const char *HTML_QUOTE_MAP[];
#define HTML_QUOTE_MASK 0x07   /* &, <, >, ", ' */
#define SHELL_UNSAFE_MASK 0x08 /* [^A-Za-z0-9_./:\200-\377] */
#define URL_QUOTE_MASK 0x10    /* [\0- \177-\377] */
#define FILE_QUOTE_MASK 0x30   /* [\0- #%&+:?\177-\377] */
#define URL_UNSAFE_MASK 0x70   /* [^A-Za-z0-9_$\-.] */
#define GET_QUOTE_TYPE(c) QUOTE_MAP[(int)(unsigned char)(c)]
#define is_html_quote(c) (GET_QUOTE_TYPE(c) & HTML_QUOTE_MASK)
#define is_shell_unsafe(c) (GET_QUOTE_TYPE(c) & SHELL_UNSAFE_MASK)
#define is_url_quote(c) (GET_QUOTE_TYPE(c) & URL_QUOTE_MASK)
#define is_file_quote(c) (GET_QUOTE_TYPE(c) & FILE_QUOTE_MASK)
#define is_url_unsafe(c) (GET_QUOTE_TYPE(c) & URL_UNSAFE_MASK)
#define html_quote_char(c) HTML_QUOTE_MAP[(int)is_html_quote(c)]

extern long long strtoclen(const char *s);
extern const char *conv_entity(unsigned int ch);
extern int getescapechar(const char **s);
extern const char *getescapecmd(const char **s);
extern int strCmp(const void *s1, const void *s2);
extern char *currentdir(void);
extern const char *cleanupName(const char *name);
#ifndef HAVE_STRCHR
extern char *strchr(const char *s, int c);
#endif /* not HAVE_STRCHR */
#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif /* not HAVE_STRCASECMP */
#ifndef HAVE_STRCASESTR
extern char *strcasestr(const char *s1, const char *s2);
#endif
extern int strcasemstr(char *str, char *srch[], char **ret_ptr);
int strmatchlen(const char *s1, const char *s2, int maxlen);
extern const char *remove_space(const char *str);
extern int non_null(const char *s);
struct Str;
extern void cleanup_line(Str *s, int mode);
extern const char *html_quote(const char *str);
extern const char *html_unquote(const char *str);
extern const char *file_quote(const char *str);
extern const char *file_unquote(const char *str);
extern const char *url_quote(const char *str);
extern Str *Str_url_unquote(Str *x, int is_form, int safe);
extern Str *Str_form_quote(Str *x);
#define Str_form_unquote(x) Str_url_unquote((x), TRUE, FALSE)
extern const char *shell_quote(const char *str);

#define xmalloc(s) xrealloc(NULL, s)
extern void *xrealloc(void *ptr, size_t size);
extern void xfree(void *ptr);
extern void *w3m_GC_realloc_atomic(void *ptr, size_t size);

extern void w3m_GC_free(void *ptr);
extern void growbuf_init(struct growbuf *gb);
extern void growbuf_init_without_GC(struct growbuf *gb);
extern void growbuf_clear(struct growbuf *gb);
extern Str *growbuf_to_Str(struct growbuf *gb);
extern void growbuf_reserve(struct growbuf *gb, int leastarea);
extern void growbuf_append(struct growbuf *gb, const unsigned char *src,
                           int len);
#define GROWBUF_ADD_CHAR(gb, ch)                                               \
  ((((gb)->length >= (gb)->area_size) ? growbuf_reserve(gb, (gb)->length + 1)  \
                                      : (void)0),                              \
   (void)((gb)->ptr[(gb)->length++] = (ch)))

extern char *w3m_auxbin_dir(void);
extern char *w3m_lib_dir(void);
extern char *w3m_etc_dir(void);
extern char *w3m_conf_dir(void);
extern char *w3m_help_dir(void);
