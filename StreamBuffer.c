#include "StreamBuffer.h"
#include <stdlib.h>
#include <string.h>

void alloc_buffer(struct StreamBuffer* sb, const uint8_t* buf, int bufsize)
{
    sb->size = bufsize;
    sb->cur = 0;
    sb->buf = malloc(bufsize);
    if (buf) {
        memcpy(sb->buf, buf, bufsize);
        sb->next = bufsize;
    } else {
        sb->next = 0;
    }
}

int buffer_read(struct StreamBuffer* sb, uint8_t* obuf, int count)
{
    int len = sb->next - sb->cur;
    if (len > 0) {
        if (len > count)
            len = count;
        memcpy(obuf, &sb->buf[sb->cur], len);
        sb->cur += len;
    }
    return len;
}
