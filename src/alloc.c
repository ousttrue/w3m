#include "alloc.h"
#include "text/Str.h"
#include <gc.h>
#include <stdlib.h>
#include <string.h>

void *_GC_MALLOC(size_t n) { return GC_MALLOC(n); }
void *_GC_MALLOC_ATOMIC(size_t n) { return GC_MALLOC_ATOMIC(n); }
void *_GC_REALLOC(void *ptr, size_t n) { return GC_REALLOC(ptr, n); }
void *_GC_ALLOC_OR_REALLOC(void *ptr, size_t size) {
  return ptr ? _GC_REALLOC(ptr, size) : _GC_MALLOC_ATOMIC(size);
}

void _GC_FREE(void *ptr) { GC_FREE(ptr); }

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

#define GC_WARN_KEEP_MAX (20)
static GC_warn_proc orig_GC_warn_proc = NULL;

static void wrap_GC_warn_proc(const char *msg, GC_word arg) {

  /* *INDENT-OFF* */
  static struct {
    const char *msg;
    GC_word arg;
  } msg_ring[GC_WARN_KEEP_MAX];
  /* *INDENT-ON* */
  static int i = 0;
  static int n = 0;
  static int lock = 0;
  int j;

  j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
  msg_ring[j].msg = msg;
  msg_ring[j].arg = arg;

  if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
    ++n;
  else
    ++i;

  if (!lock) {
    lock = 1;

    for (; n > 0; --n, ++i) {
      i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

      printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
      // tty_sleep_till_anykey(1, 1);
    }

    lock = 0;
  }

  // else if (orig_GC_warn_proc)
  //   orig_GC_warn_proc(msg, arg);
  // else
  //   fprintf(stderr, msg, (unsigned long)arg);
}

static void *die_oom(size_t bytes) {
  fprintf(stderr, "Out of memory: %lu bytes unavailable!\n",
          (unsigned long)bytes);
  exit(1);
}

void _GC_INIT() {
  GC_INIT();
#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  GC_set_oom_fn(die_oom);
#else
  GC_oom_fn = die_oom;
#endif

#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  orig_GC_warn_proc = GC_get_warn_proc();
  GC_set_warn_proc(wrap_GC_warn_proc);
#else
  orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif
}
