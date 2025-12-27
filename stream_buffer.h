#pragma once

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4

struct stream_buffer {
    unsigned char* buf;
    int size, cur, next;
};
