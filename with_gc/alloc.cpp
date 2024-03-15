#include "alloc.h"

/* Define this as a separate function in case the free() has
 * an incompatible prototype. */
void xfree(void *ptr) { free(ptr); }
