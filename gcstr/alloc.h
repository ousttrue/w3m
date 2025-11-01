#pragma once
#include <stddef.h>

void alloc_init();

size_t
z_mult_no_oflow_(size_t n, size_t size);

void* w3m_GC_alloc(size_t size);
void* w3m_GC_alloc_atomic(size_t size);
void* w3m_GC_realloc(void* ptr, size_t size);
void* w3m_GC_realloc_atomic(void* ptr, size_t size);
void w3m_GC_free(void* ptr);

#define New(type) \
    (w3m_GC_alloc(sizeof(type)))

#define NewAtom(type) \
    (w3m_GC_alloc_atomic(sizeof(type)))

#define New_N(type, n) \
    (w3m_GC_alloc(z_mult_no_oflow_((n), sizeof(type))))

#define NewAtom_N(type, n) \
    (w3m_GC_alloc_atomic(z_mult_no_oflow_((n), sizeof(type))))

#define New_Reuse(type, ptr, n) \
    (w3m_GC_realloc((ptr), z_mult_no_oflow_((n), sizeof(type))))

void* xrealloc(void* ptr, size_t size);

static inline void* xmalloc(size_t s)
{
    return xrealloc(NULL, s);
}

/* Define this as a separate function in case the free() has
 * an incompatible prototype. */
void xfree(void* ptr);

#define NewWithoutGC(type) ((type*)xmalloc(sizeof(type)))
#define NewWithoutGC_N(type, n) ((type*)xmalloc((n) * sizeof(type)))
#define NewWithoutGC_Reuse(type, ptr, n) ((type*)xrealloc(ptr, (n) * sizeof(type)))

struct growbuf {
    char* ptr;
    int length;
    int area_size;
    void* (*realloc_proc)(void*, size_t);
    void (*free_proc)(void*);
};

extern void growbuf_init(struct growbuf* gb);
extern void growbuf_init_without_GC(struct growbuf* gb);
extern void growbuf_clear(struct growbuf* gb);
extern void growbuf_reserve(struct growbuf* gb, int leastarea);
extern void growbuf_append(struct growbuf* gb, const unsigned char* src, int len);
#define GROWBUF_ADD_CHAR(gb, ch) ((((gb)->length >= (gb)->area_size) ? growbuf_reserve(gb, (gb)->length + 1) : (void)0), (void)((gb)->ptr[(gb)->length++] = (ch)))

extern struct Str* growbuf_to_Str(struct growbuf* gb);
