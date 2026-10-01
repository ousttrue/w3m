#include "growbuf.h"
#include "alloc.h"
#include <gc/gc.h>
#include <stdlib.h>

void* w3m_GC_realloc_atomic(void* ptr, size_t size)
{
    return ptr ? GC_REALLOC(ptr, size) : GC_MALLOC_ATOMIC(size);
}

void w3m_GC_free(void* ptr)
{
    GC_FREE(ptr);
}

void growbuf_init(struct growbuf* gb)
{
    gb->ptr = NULL;
    gb->len = 0;
    gb->capacity = 0;
    gb->realloc_proc = &w3m_GC_realloc_atomic;
    gb->free_proc = &w3m_GC_free;
}

void growbuf_init_without_GC(struct growbuf* gb)
{
    gb->ptr = NULL;
    gb->len = 0;
    gb->capacity = 0;
    gb->realloc_proc = &xrealloc;
    gb->free_proc = &free;
}

void growbuf_clear(struct growbuf* gb)
{
    (*gb->free_proc)(gb->ptr);
    gb->ptr = NULL;
    gb->len = 0;
    gb->capacity = 0;
}

pStr growbuf_to_Str(struct growbuf* gb)
{
    pStr s;

    if (gb->free_proc == &w3m_GC_free) {
        growbuf_reserve(gb, gb->len + 1);
        gb->ptr[gb->len] = '\0';
        s = New(struct Str);
        s->ptr = gb->ptr;
        s->len = gb->len;
        s->capacity = gb->capacity;
    } else {
        s = Strnew_charp_n(gb->ptr, gb->len);
        (*gb->free_proc)(gb->ptr);
    }
    gb->ptr = NULL;
    gb->len = 0;
    gb->capacity = 0;
    return s;
}

void growbuf_reserve(struct growbuf* gb, int leastarea)
{
    int newarea;

    if (gb->capacity < leastarea) {
        newarea = gb->capacity * 3 / 2;
        if (newarea < leastarea)
            newarea = leastarea;
        newarea += 16;
        gb->ptr = (*gb->realloc_proc)(gb->ptr, newarea);
        gb->capacity = newarea;
    }
}

void growbuf_append(struct growbuf* gb, const unsigned char* src, int len)
{
    growbuf_reserve(gb, gb->len + len);
    memcpy(&gb->ptr[gb->len], src, len);
    gb->len += len;
}
