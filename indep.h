#pragma once
#include "Str.h"
#include <stdint.h>
#include <stdbool.h>

struct growbuf {
    char* ptr;
    int length;
    int area_size;
    void* (*realloc_proc)(void*, size_t);
    void (*free_proc)(void*);
};

extern char* conv_entity(unsigned int ch);
extern int getescapechar(char** s);
extern char* getescapecmd(char** s);
extern char* allocStr(const char* s, int len);
extern int strCmp(const void* s1, const void* s2);
extern char* currentdir(void);
extern char* cleanupName(const char* name);
extern char* expandPath(const char* name);
extern char* remove_space(const char* str);
extern bool non_null(const char* s);
extern char* html_quote(const char* str);
extern char* html_unquote(const char* str);
extern char* file_quote(char* str);
extern char* file_unquote(const char* str);
extern char* url_quote(const char* str);
extern Str Str_url_unquote(Str x, int is_form, int safe);
extern Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
extern char* shell_quote(const char* str);

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
