#pragma once
#include <stdint.h>

#define STREAM_BUF_SIZE 8192
struct StreamBuffer {
    uint8_t* buf;
    int size;
    int cur;
    int next;
};

void alloc_buffer(struct StreamBuffer* sb, const uint8_t* buf, int bufsize);
int buffer_read(struct StreamBuffer* sb, char* obuf, int count);
