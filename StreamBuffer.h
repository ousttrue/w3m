#pragma once
#include <stdint.h>
#include <stdbool.h>

#define STREAM_BUF_SIZE 8192
struct StreamBuffer {
    uint8_t* buf;
    /// buffer length
    int capacity;
    /// available length(length<=capacity)
    int length;
    /// read position(cur<=length)
    int cur;
};
static inline bool MUST_BE_UPDATED(struct StreamBuffer* sb)
{
    return sb->cur == sb->length;
}
void alloc_buffer(struct StreamBuffer* sb, const uint8_t* buf, int bufsize);
int buffer_read(struct StreamBuffer* sb, uint8_t* obuf, int count);
