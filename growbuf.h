#pragma once
// #include "Str.h"
#include <stdint.h>
#include <stddef.h>

typedef void* (*GrowbufReallocFunc)(void*, size_t);
typedef void (*GrowbufFreeFunc)(void*);

struct growbuf {
    uint8_t* ptr;
    int length;
    int area_size;
    GrowbufReallocFunc realloc_proc;
    GrowbufFreeFunc free_proc;
};

void growbuf_init(struct growbuf* gb);
void growbuf_init_without_GC(struct growbuf* gb);
void growbuf_clear(struct growbuf* gb);
// Str growbuf_to_Str(struct growbuf* gb);
void growbuf_reserve(struct growbuf* gb, int leastarea);
void growbuf_append(struct growbuf* gb, const unsigned char* src, int len);

static inline void GROWBUF_ADD_CHAR(struct growbuf* gb, int ch)
{
    if (gb->length >= gb->area_size) {
        growbuf_reserve(gb, gb->length + 1);
    }
    gb->ptr[gb->length++] = ch;
}
