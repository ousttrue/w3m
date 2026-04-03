#pragma once
#include "wc_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct MallocBuffer {
    char* buf;
    int capacity;
    int len;
};
static inline char* MallocBuffer_getPtr(void* data)
{
    struct MallocBuffer* b = (struct MallocBuffer*)data;
    return b->buf;
}
static inline int MallocBuffer_getLen(void* data)
{
    struct MallocBuffer* b = (struct MallocBuffer*)data;
    return b->len;
}
static inline void MallocBuffer_putc(void* data, uint8_t ch)
{
    struct MallocBuffer* b = (struct MallocBuffer*)data;
    if (b->len >= b->capacity) {
        int len = b->capacity ? b->capacity * 2 : 16;
        char* buf = (char*)malloc(len);
        if (b->len) {
            memcpy(buf, b->buf, b->len);
            free(b->buf);
        }
        b->buf = buf;
        b->capacity = len;
    }
    b->buf[b->len++] = ch;
}
static inline void MallocBuffer_clear(void* data)
{
    struct MallocBuffer* b = (struct MallocBuffer*)data;
    memset(b->buf, 0, b->capacity);
    b->len = 0;
}
static inline void MallocBuffer_free(void* data)
{
    struct MallocBuffer* b = (struct MallocBuffer*)data;
    free(b->buf);
    *b = (struct MallocBuffer) { 0 };
}

//
// public
//
static inline struct wc_output MallocBuffer_from_charp_n(const char* is, int n)
{
    char* buf = (char*)malloc(n + 1);
    if (is) {
        memcpy(buf, is, n);
        buf[n] = 0;
    } else {
        memset(buf, 0, n + 1);
        n = 0;
    }

    struct wc_output os = {
        .getPtr = MallocBuffer_getPtr,
        .getLen = MallocBuffer_getLen,
        .putc = MallocBuffer_putc,
        .clear = MallocBuffer_clear,
        .free = MallocBuffer_free,
    };

    os.data = malloc(sizeof(struct MallocBuffer));
    *((struct MallocBuffer*)os.data) = (struct MallocBuffer) {
        .buf = buf,
        .capacity = n+1,
        .len = n,
    };

    return os;
}
