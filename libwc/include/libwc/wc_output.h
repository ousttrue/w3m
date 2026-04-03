#pragma once
#include "wc_types.h"
#include <string.h>
#include "malloc_buffer.h"

struct wc_status;
void wc_output_init(struct wc_option opts, wc_ces ces, struct wc_status* st);
typedef void (*PushToFunc)(struct wc_option opts, struct wc_output* os, struct wc_wchar, struct wc_status* st);
void wc_push_end(struct wc_output* os, struct wc_status* st);

static inline struct wc_output wc_output_from_charp_n(const char* is, int n)
{
    return MallocBuffer_from_charp_n(is, n);
}
static inline struct wc_output wc_output_from_charp(const char* is)
{
    return MallocBuffer_from_charp_n(is, strlen(is));
}
static inline struct wc_output wc_output_from_size(int n)
{
    return MallocBuffer_from_charp_n(0, n);
}

static inline char* wc_output__ptr(struct wc_output os)
{
    return os.getPtr(os.data);
}

static inline int wc_output__len(struct wc_output os)
{
    return os.getLen(os.data);
}

static inline void wc_output__free(struct wc_output* os)
{
    os->free(os->data);
}
static inline void wc_output__clear(struct wc_output* os)
{
    os->clear(os->data);
}
static inline void wc_output__cat_char(struct wc_output* os, char ch)
{
    os->putc(os->data, (uint8_t)ch);
}
static inline void wc_output__cat_charp_n(struct wc_output* os, const char* is, int len)
{
    for (int i = 0; i < len; ++i, ++is) {
        os->putc(os->data, (uint8_t)*is);
    }
}
static inline void wc_output__cat_charp(struct wc_output* os, const char* is)
{
    wc_output__cat_charp_n(os, is, strlen(is));
}
static inline void wc_output__copy_charp_n(struct wc_output* os, const char* is, int len)
{
    wc_output__clear(os);
    wc_output__cat_charp_n(os, is, len);
}
