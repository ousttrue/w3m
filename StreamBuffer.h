#pragma once
#include <stdint.h>
#include <stdbool.h>

#define STREAM_BUF_SIZE 8192
struct StreamBuffer {
    uint8_t* buf;
    int capacity;
    int cur;
    int next;
};
static inline bool MUST_BE_UPDATED(struct StreamBuffer* sb)
{
    return sb->cur == sb->next;
}
void alloc_buffer(struct StreamBuffer* sb, const uint8_t* buf, int bufsize);
int buffer_read(struct StreamBuffer* sb, uint8_t* obuf, int count);
