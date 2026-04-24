#include "growbuf.h"
// #include "alloc.h"
#include <stdlib.h>
#include <string.h>

typedef void* (*GrowbufReallocFunc)(void*, size_t);
typedef void (*GrowbufFreeFunc)(void*);

struct growbuf {
    uint8_t* ptr;
    int length;
    int area_size;
    GrowbufReallocFunc realloc_proc;
    GrowbufFreeFunc free_proc;
};

static void* w3m_GC_realloc_atomic(void* ptr, size_t size)
{
    return ptr ? realloc(ptr, size) : malloc(size);
}

static void w3m_GC_free(void* ptr)
{
    free(ptr);
}

struct growbuf* growbuf_create()
{
    struct growbuf* gb = malloc(sizeof(struct growbuf));
    *gb = (struct growbuf) {
        .ptr = NULL,
        .length = 0,
        .area_size = 0,
        .realloc_proc = &w3m_GC_realloc_atomic,
        .free_proc = &w3m_GC_free,
    };
    return gb;
}

void growbuf_destroy(struct growbuf* gb)
{
    free(gb->ptr);
    gb->ptr = NULL;
    free(gb);
}

struct str_view growbuf_str_view(struct growbuf* gb)
{
    struct str_view gv = {
        .ptr = (char*)gb->ptr,
        .len = gb->length,
    };
    while (gv.len > 0 && gv.ptr[gv.len - 1] == '\0') {
        gv.len--;
    }
    return gv;
}

struct span growbuf_span(struct growbuf* gb)
{
    return (struct span) {
        .ptr = gb->ptr,
        .len = gb->length,
    };
}

void growbuf_clear(struct growbuf* gb)
{
    gb->length = 0;
}

void growbuf_reserve(struct growbuf* gb, int leastarea)
{
    if (gb->area_size < leastarea) {
        int newarea = gb->area_size * 3 / 2;
        if (newarea < leastarea)
            newarea = leastarea;
        newarea += 16;
        gb->ptr = (*gb->realloc_proc)(gb->ptr, newarea);
        gb->area_size = newarea;
    }
}

void growbuf_append(struct growbuf* gb, const unsigned char* src, int len)
{
    growbuf_reserve(gb, gb->length + len);
    memcpy(&gb->ptr[gb->length], src, len);
    gb->length += len;
}

void growbuf_add_char(struct growbuf* gb, int ch)
{
    if (gb->length >= gb->area_size) {
        growbuf_reserve(gb, gb->length + 1);
    }
    gb->ptr[gb->length++] = ch;
}
