#include "stream_buffer.h"
#include <string.h>

int buffer_read(struct stream_buffer *sb, char *obuf, int count) {
  int len = sb->next - sb->cur;
  if (len > 0) {
    if (len > count)
      len = count;
    memcpy(obuf, (const void *)&sb->buf[sb->cur], len);
    sb->cur += len;
  }
  return len;
}
