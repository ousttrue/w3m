#pragma once
#include <gc.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

static inline size_t z_mult_no_oflow_(size_t n, size_t size) {
  if (size != 0 && n > ULONG_MAX / size) {
    fprintf(stderr, "w3m: overflow in malloc, %lu*%lu\n", (unsigned long)n,
            (unsigned long)size);
    exit(1);
  }
  return n * size;
}

#define New(type) (type*)(GC_MALLOC(sizeof(type)))

#define NewAtom(type) (GC_MALLOC_ATOMIC(sizeof(type)))

#define New_N(type, n) (GC_MALLOC(z_mult_no_oflow_((n), sizeof(type))))

#define NewAtom_N(type, n)                                                     \
  (GC_MALLOC_ATOMIC(z_mult_no_oflow_((n), sizeof(type))))

#define New_Reuse(type, ptr, n)                                                \
  (GC_REALLOC((ptr), z_mult_no_oflow_((n), sizeof(type))))

extern void *xrealloc(void *ptr, size_t size);
extern void xfree(void *ptr);
#define xmalloc(s) xrealloc(NULL, s)
#define NewWithoutGC(type) ((type *)xmalloc(sizeof(type)))
#define NewWithoutGC_N(type, n) ((type *)xmalloc((n) * sizeof(type)))
#define NewWithoutGC_Reuse(type, ptr, n)                                       \
  ((type *)xrealloc(ptr, (n) * sizeof(type)))

extern char *allocStr(const char *s, int len);
