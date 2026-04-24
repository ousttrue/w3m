#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct str_view {
    char* ptr;
    size_t len;
};

inline static struct str_view sv_chop(struct str_view v)
{
    char* q;
    for (q = v.ptr + v.len; q > v.ptr; --q) {
        if (q[-1] != '\n' && q[-1] != '\r')
            break;
    }
    if (q == v.ptr + v.len) {
        return v;
    }
    return (struct str_view) {
        .ptr = v.ptr,
        .len = q - v.ptr,
    };
}
