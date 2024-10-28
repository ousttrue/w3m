#pragma once

#define STREAM_BUF_SIZE 8192

struct stream_buffer {
  unsigned char *buf;
  int size, cur, next;
};
int buffer_read(struct stream_buffer *sb, char *obuf, int count);
