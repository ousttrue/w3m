#pragma once
#include <stdint.h>
#include <stddef.h>

struct growbuf;
struct growbuf* growbuf_create();
void growbuf_destroy(struct growbuf* gb);

struct span {
    uint8_t* ptr;
    size_t len;
};

inline static struct span sv_chop(struct span v)
{
    uint8_t* q;
    for (q = v.ptr + v.len; q > v.ptr; --q) {
        if (q[-1] != '\n' && q[-1] != '\r' && q[-1] != 0)
            break;
    }
    if (q == v.ptr + v.len) {
        return v;
    }
    return (struct span) {
        .ptr = v.ptr,
        .len = q - v.ptr,
    };
}
struct span growbuf_span(struct growbuf* gb);

void growbuf_clear(struct growbuf* gb);
void growbuf_reserve(struct growbuf* gb, int leastarea);
void growbuf_append(struct growbuf* gb, const unsigned char* src, size_t len);
void growbuf_add_char(struct growbuf* gb, int ch);
