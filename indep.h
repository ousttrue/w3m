#pragma once
#include "alloc.h"
#include "Str.h"
#include <stdbool.h>

#define HAVE_STRTOLL 1
#define HAVE_ATOLL 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCASESTR 1
#define HAVE_STRCHR 1

struct growbuf {
    char* ptr;
    int length;
    int area_size;
    void* (*realloc_proc)(void*, size_t);
    void (*free_proc)(void*);
};

#define RAW_MODE 0
#define PAGER_MODE 1
#define HTML_MODE 2
#define HEADER_MODE 3

extern char* HTML_QUOTE_MAP[];
#define html_quote_char(c) HTML_QUOTE_MAP[(int)is_html_quote(c)]

extern int64_t strtoclen(const char* s);
extern char* conv_entity(unsigned int ch);
extern int getescapechar(char** s);
extern char* getescapecmd(char** s);
extern char* allocStr(const char* s, int len);
extern int strCmp(const void* s1, const void* s2);
extern char* currentdir(void);
extern char* cleanupName(const char* name);
extern char* expandPath(const char* name);
#ifndef HAVE_STRCHR
extern char* strchr(const char* s, int c);
#endif /* not HAVE_STRCHR */
#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char* s1, const char* s2);
extern int strncasecmp(const char* s1, const char* s2, size_t n);
#endif /* not HAVE_STRCASECMP */
#ifndef HAVE_STRCASESTR
extern char* strcasestr(const char* s1, const char* s2);
#endif
extern int strcasemstr(char* str, char* srch[], char** ret_ptr);
int strmatchlen(const char* s1, const char* s2, int maxlen);
extern char* remove_space(const char* str);
extern bool non_null(const char* s);
extern void cleanup_line(Str s, int mode);
extern char* html_quote(const char* str);
extern char* html_unquote(const char* str);
extern char* file_quote(char* str);
extern char* file_unquote(const char* str);
extern char* url_quote(const char* str);
extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
extern char* shell_quote(const char* str);
#define xmalloc(s) xrealloc(NULL, s)
extern void* xrealloc(void* ptr, size_t size);
extern void xfree(void* ptr);
extern void* w3m_GC_realloc_atomic(void* ptr, size_t size);
extern void w3m_GC_free(void* ptr);
extern void growbuf_init(struct growbuf* gb);
extern void growbuf_init_without_GC(struct growbuf* gb);
extern void growbuf_clear(struct growbuf* gb);
extern Str growbuf_to_Str(struct growbuf* gb);
extern void growbuf_reserve(struct growbuf* gb, int leastarea);
extern void growbuf_append(struct growbuf* gb, const unsigned char* src, int len);
#define GROWBUF_ADD_CHAR(gb, ch) ((((gb)->length >= (gb)->area_size) ? growbuf_reserve(gb, (gb)->length + 1) : (void)0), (void)((gb)->ptr[(gb)->length++] = (ch)))

extern char* w3m_auxbin_dir(void);
extern char* w3m_lib_dir(void);
extern char* w3m_etc_dir(void);
extern char* w3m_conf_dir(void);
extern char* w3m_help_dir(void);

#define NewWithoutGC(type) ((type*)xmalloc(sizeof(type)))
#define NewWithoutGC_N(type, n) ((type*)xmalloc((n) * sizeof(type)))
#define NewWithoutGC_Reuse(type, ptr, n) ((type*)xrealloc(ptr, (n) * sizeof(type)))

