#pragma once
#include <gc/gc.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

static inline size_t
z_mult_no_oflow_(size_t n, size_t size)
{
    if (size != 0 && n > SIZE_MAX / size) {
        fprintf(stderr,
            "w3m: overflow in malloc, %zu*%zu\n", n, size);
        exit(3);
    }
    return n * size;
}

//
// with GC
//
#define New(type) \
    (GC_MALLOC(sizeof(type)))

#define NewAtom(type) \
    (GC_MALLOC_ATOMIC(sizeof(type)))

#define New_N(type, n) \
    (GC_MALLOC(z_mult_no_oflow_((n), sizeof(type))))

#define NewAtom_N(type, n) \
    (GC_MALLOC_ATOMIC(z_mult_no_oflow_((n), sizeof(type))))

#define New_Reuse(type, ptr, n) \
    (GC_REALLOC((ptr), z_mult_no_oflow_((n), sizeof(type))))

//
// without GC
//
void* xrealloc(void* ptr, size_t size);
static inline void* xmalloc(size_t s)
{
    return xrealloc(NULL, s);
}
#define NewWithoutGC(type) ((type*)xmalloc(sizeof(type)))
#define NewWithoutGC_N(type, n) ((type*)xmalloc((n) * sizeof(type)))
#define NewWithoutGC_Reuse(type, ptr, n) ((type*)xrealloc(ptr, (n) * sizeof(type)))
