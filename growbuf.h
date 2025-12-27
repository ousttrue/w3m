#pragma once
#include "Str.h"

struct growbuf {
    char* ptr;
    int length;
    int area_size;
    void* (*realloc_proc)(void*, size_t);
    void (*free_proc)(void*);
};

void growbuf_init(struct growbuf* gb);
void growbuf_init_without_GC(struct growbuf* gb);
void growbuf_clear(struct growbuf* gb);
Str growbuf_to_Str(struct growbuf* gb);
void growbuf_reserve(struct growbuf* gb, int leastarea);
void growbuf_append(struct growbuf* gb, const unsigned char* src, int len);
#define GROWBUF_ADD_CHAR(gb, ch) ((((gb)->length >= (gb)->area_size) ? growbuf_reserve(gb, (gb)->length + 1) : (void)0), (void)((gb)->ptr[(gb)->length++] = (ch)))
