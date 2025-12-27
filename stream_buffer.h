#pragma once
#include <stdint.h>

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4

struct stream_buffer {
    uint8_t* buf;
    int size;
    int cur;
    int next;
};
