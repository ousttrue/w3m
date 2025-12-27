#pragma once
#include <stdbool.h>
#include <stdint.h>

enum InputStreamType {
    IST_BASIC = 0,
    IST_FILE = 1,
    IST_STR = 2,
    IST_SSL = 3,
};

struct stream_buffer {
    uint8_t* buf;
    int size;
    int cur;
    int next;
};

inline static bool MUST_BE_UPDATED(struct stream_buffer* sb)
{
    return (sb->cur == sb->next);
}
