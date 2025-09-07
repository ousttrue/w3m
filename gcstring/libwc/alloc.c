#include "alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

size_t z_mult_no_oflow_(size_t n, size_t size)
{
    if (size != 0 && n > ULONG_MAX / size) {
        fprintf(stderr,
            "w3m: overflow in malloc, %lu*%lu\n", (unsigned long)n, (unsigned long)size);
        exit(1);
    }
    return n * size;
}
