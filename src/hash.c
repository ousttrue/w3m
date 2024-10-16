#include "hash.h"
#include <gc.h>
#include <string.h>

static unsigned int hashfunc(const char *s) {
  unsigned int h = 0;
  while (*s) {
    if (h & 0x80000000) {
      h <<= 1;
      h |= 1;
    } else
      h <<= 1;
    h += *s;
    s++;
  }
  return h;
}

#define keycomp(x, y) !strcmp(x, y)

defhashfunc(const char *, int, si) 
defhashfunc(const char *, const char *, ss)
defhashfunc(const char *, void *, sv) 
defhashfunc_i(int, void *, iv)
