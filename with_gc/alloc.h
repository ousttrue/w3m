/*
 * by Scarlett. public domain.
 * replacements for w3m's allocation macros which add overflow
 * detection and concentrate the macros in one file
 */
#ifndef W3_ALLOC_H
#define W3_ALLOC_H
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

#define New(type) (GC_MALLOC(sizeof(type)))

#define NewAtom(type) (GC_MALLOC_ATOMIC(sizeof(type)))

#define New_N(type, n) (GC_MALLOC(z_mult_no_oflow_((n), sizeof(type))))

#define NewAtom_N(type, n)                                                     \
  (GC_MALLOC_ATOMIC(z_mult_no_oflow_((n), sizeof(type))))

#define New_Reuse(type, ptr, n)                                                \
  (GC_REALLOC((ptr), z_mult_no_oflow_((n), sizeof(type))))

inline void *xrealloc(void *ptr, size_t size) {
  void *newptr = realloc(ptr, size);
  if (newptr == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(-1);
  }
  return newptr;
}

#define NewWithoutGC(type) ((type *)xrealloc(NULL, sizeof(type)))
#define NewWithoutGC_N(type, n) ((type *)xrealloc(NULL, (n) * sizeof(type)))
#define NewWithoutGC_Reuse(type, ptr, n)                                       \
  ((type *)xrealloc(ptr, (n) * sizeof(type)))

#endif /* W3_ALLOC_H */

void xfree(void *ptr);

