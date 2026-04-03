#pragma once
#include "wc_types.h"
#include <string.h>
#include <stdint.h>

// for 7byte
struct FixedBuffer {
    char buf[8];
};

//
// interface
//
static inline char* FixedBuffer_getPtr(void* data)
{
    struct FixedBuffer* b = (struct FixedBuffer*)&data;
    return b->buf;
}
static inline int FixedBuffer_getLen(void* data)
{
    struct FixedBuffer* b = (struct FixedBuffer*)&data;
    return strlen(b->buf);
}
static inline void FixedBuffer_putc(void* data, uint8_t ch)
{
    struct FixedBuffer* b = (struct FixedBuffer*)&data;
    size_t len = strlen(b->buf);
    b->buf[len] = ch;
    b->buf[len + 1] = 0;
}
static inline void FixedBuffer_clear(void* data)
{
    struct FixedBuffer* b = (struct FixedBuffer*)&data;
    *b = (struct FixedBuffer) { 0 };
}

//
// public
//
static inline struct wc_output FixedBuffer_from_charp_n(const char* is, int n)
{
    struct wc_output os;

    struct FixedBuffer b = { };
    if (is) {
        memcpy(b.buf, is, n);
        b.buf[n] = 0;
    } else {
        memset(b.buf, 0, n);
    }
    *((struct FixedBuffer*)os.data) = b;
    os.getPtr = FixedBuffer_getPtr;
    os.getLen = FixedBuffer_getLen;
    os.putc = FixedBuffer_putc;
    os.clear = FixedBuffer_clear;
    os.free = FixedBuffer_clear;

    return os;
}
