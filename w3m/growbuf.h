#pragma once
#include <stdint.h>
#include <stddef.h>
#include "str_view.h"

struct growbuf;
struct growbuf* growbuf_create();
void growbuf_destroy(struct growbuf* gb);
struct str_view growbuf_str_view(struct growbuf* gb);

struct span {
    uint8_t* ptr;
    size_t len;
};
struct span growbuf_span(struct growbuf* gb);

void growbuf_clear(struct growbuf* gb);
void growbuf_reserve(struct growbuf* gb, int leastarea);
void growbuf_append(struct growbuf* gb, const unsigned char* src, size_t len);
void growbuf_add_char(struct growbuf* gb, int ch);
