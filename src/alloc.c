#include "alloc.h"
#include "text/Str.h"
#include <stdlib.h>
#include <string.h>

char *allocStr(const char *s, int len) {
  char *ptr;

  if (s == NULL)
    return NULL;
  if (len < 0)
    len = strlen(s);
  if (len < 0 || len >= STR_SIZE_MAX)
    len = STR_SIZE_MAX - 1;
  ptr = NewAtom_N(char, len + 1);
  if (ptr == NULL) {
    fprintf(stderr, "fm: Can't allocate string. Give me more memory!\n");
    exit(-1);
  }
  memcpy(ptr, s, len);
  ptr[len] = '\0';
  return ptr;
}

/* Define this as a separate function in case the free() has
 * an incompatible prototype. */
void xfree(void *ptr) { free(ptr); }

void *xrealloc(void *ptr, size_t size) {
  void *newptr = realloc(ptr, size);
  if (newptr == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(-1);
  }
  return newptr;
}
