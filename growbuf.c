#include "growbuf.h"
#include "alloc.h"

void *w3m_GC_realloc_atomic(void *ptr, size_t size) {
  return ptr ? GC_REALLOC(ptr, size) : GC_MALLOC_ATOMIC(size);
}

void w3m_GC_free(void *ptr) { GC_FREE(ptr); }

void growbuf_init(struct growbuf *gb) {
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
  gb->realloc_proc = &w3m_GC_realloc_atomic;
  gb->free_proc = &w3m_GC_free;
}

void growbuf_init_without_GC(struct growbuf *gb) {
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
  gb->realloc_proc = &xrealloc;
  gb->free_proc = &xfree;
}

void growbuf_clear(struct growbuf *gb) {
  (*gb->free_proc)(gb->ptr);
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
}

Str growbuf_to_Str(struct growbuf *gb) {
  Str s;

  if (gb->free_proc == &w3m_GC_free) {
    growbuf_reserve(gb, gb->length + 1);
    gb->ptr[gb->length] = '\0';
    s = New(struct _Str);
    s->ptr = gb->ptr;
    s->length = gb->length;
    s->area_size = gb->area_size;
  } else {
    s = Strnew_charp_n(gb->ptr, gb->length);
    (*gb->free_proc)(gb->ptr);
  }
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
  return s;
}

void growbuf_reserve(struct growbuf *gb, int leastarea) {
  int newarea;

  if (gb->area_size < leastarea) {
    newarea = gb->area_size * 3 / 2;
    if (newarea < leastarea)
      newarea = leastarea;
    newarea += 16;
    gb->ptr = (*gb->realloc_proc)(gb->ptr, newarea);
    gb->area_size = newarea;
  }
}

void growbuf_append(struct growbuf *gb, const char *src, int len) {
  growbuf_reserve(gb, gb->length + len);
  memcpy(&gb->ptr[gb->length], src, len);
  gb->length += len;
}
