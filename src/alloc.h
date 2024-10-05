#pragma once
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

void _GC_INIT();
void *_GC_MALLOC(size_t n);
void *_GC_MALLOC_ATOMIC(size_t n);
void *_GC_REALLOC(void *ptr, size_t n);
void *_GC_ALLOC_OR_REALLOC(void *ptr, size_t size);
void _GC_FREE(void *ptr);

static inline size_t z_mult_no_oflow_(size_t n, size_t size) {
  if (size != 0 && n > ULONG_MAX / size) {
    fprintf(stderr, "w3m: overflow in malloc, %lu*%lu\n", (unsigned long)n,
            (unsigned long)size);
    exit(1);
  }
  return n * size;
}

#define New(type) (type *)(_GC_MALLOC(sizeof(type)))
#define NewAtom(type) (type *)(_GC_MALLOC_ATOMIC(sizeof(type)))
#define New_N(type, n) (type *)(_GC_MALLOC(z_mult_no_oflow_((n), sizeof(type))))
#define NewAtom_N(type, n)                                                     \
  (_GC_MALLOC_ATOMIC(z_mult_no_oflow_((n), sizeof(type))))

#define New_Reuse(type, ptr, n)                                                \
  (_GC_REALLOC((ptr), z_mult_no_oflow_((n), sizeof(type))))

extern void *xrealloc(void *ptr, size_t size);
extern void xfree(void *ptr);
#define xmalloc(s) xrealloc(NULL, s)
#define NewWithoutGC(type) ((type *)xmalloc(sizeof(type)))
#define NewWithoutGC_N(type, n) ((type *)xmalloc((n) * sizeof(type)))
#define NewWithoutGC_Reuse(type, ptr, n)                                       \
  ((type *)xrealloc(ptr, (n) * sizeof(type)))

extern char *allocStr(const char *s, int len);
