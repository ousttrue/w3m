#pragma once
#include "Str.h"

typedef void* (*ReallocFunc)(void*, size_t);
typedef void (*FreeFunc)(void*);
struct growbuf {
    char* ptr;
    int length;
    int area_size;
    ReallocFunc realloc_proc;
    FreeFunc free_proc;
};

extern void growbuf_init(struct growbuf* gb);
extern void growbuf_init_without_GC(struct growbuf* gb);
extern void growbuf_clear(struct growbuf* gb);
extern Str growbuf_to_Str(struct growbuf* gb);
extern void growbuf_reserve(struct growbuf* gb, int leastarea);
extern void growbuf_append(struct growbuf* gb, const unsigned char* src, int len);
inline static void GROWBUF_ADD_CHAR(struct growbuf* gb, char ch)
{
    ((((gb)->length >= (gb)->area_size) ? growbuf_reserve(gb, (gb)->length + 1) : (void)0), (void)((gb)->ptr[(gb)->length++] = (ch)));
}
