#pragma once
#include "Str.h"
#include <libwc/wc_types.h>
#include <libwc/wc_output.h>
#include <libwc/ces.h>
#include <libwc/conv.h>

extern struct wc_option WcOption;

char* url_quote_conv(char* is, wc_ces c);

Str Str_conv_from_system(const char* is, int len);
Str Str_conv_to_system(const char* is, int len);
Str Str_conv_to_halfdump(struct wc_option opts, Str x);
char* conv_from_system(const char* x);
char* conv_to_system(const char* x);

static inline Str Strnew_wc_output(struct wc_output os)
{
    return Strnew_charp_n(
        wc_output__ptr(os),
        wc_output__len(os));
}
int get_Str_strwidth(Str s);
int get_mcwidth(const char* p);

static inline char* StrBuffer_getPtr(void* data)
{
    Str s = (Str)data;
    return s->ptr;
}
static inline int StrBuffer_getLen(void* data)
{
    Str s = (Str)data;
    return s->length;
}
static inline void StrBuffer_putc(void* data, uint8_t ch)
{
    Str s = (Str)data;
    Strcat_char(s, ch);
}
static inline void StrBuffer_clear(void* data)
{
    Str s = (Str)data;
    Strclear(s);
}
static inline void StrBuffer_free(void* data)
{
    Str s = (Str)data;
    Strfree(s);
}
static inline struct wc_output StrBuffer_from_str(Str str)
{
    return (struct wc_output) {
        .data = str,
        .getPtr = StrBuffer_getPtr,
        .getLen = StrBuffer_getLen,
        .putc = StrBuffer_putc,
        .clear = StrBuffer_clear,
        .free = StrBuffer_free,
    };
}
