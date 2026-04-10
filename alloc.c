#include "alloc.h"

void* xrealloc(void* ptr, size_t size)
{
    void* newptr = realloc(ptr, size);
    if (newptr == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    return newptr;
}

/* Define this as a separate function in case the free() has
 * an incompatible prototype. */
void xfree(void* ptr)
{
    free(ptr);
}
