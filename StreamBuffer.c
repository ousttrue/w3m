#include "StreamBuffer.h"
#include <stdlib.h>
#include <string.h>

void alloc_buffer(struct StreamBuffer* sb, const uint8_t* buf, int bufsize)
{
    sb->buf = malloc(bufsize);
    sb->capacity = bufsize;
    sb->cur = 0;

    if (buf) {
        memcpy(sb->buf, buf, bufsize);
        sb->length = bufsize;
    } else {
        sb->length = 0;
    }
}

int buffer_read(struct StreamBuffer* sb, uint8_t* obuf, int count)
{
    int len = sb->length - sb->cur;
    if (len > 0) {
        if (len > count)
            len = count;
        memcpy(obuf, &sb->buf[sb->cur], len);
        sb->cur += len;
    }
    return len;
}
