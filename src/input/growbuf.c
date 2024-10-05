#include "growbuf.h"
#include "alloc.h"

void growbuf_init(struct growbuf *gb) {
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
  gb->realloc_proc = &_GC_ALLOC_OR_REALLOC;
  gb->free_proc = &_GC_FREE;
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

  if (gb->free_proc == &_GC_FREE) {
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

void growbuf_append(struct growbuf *gb, const unsigned char *src, int len) {
  growbuf_reserve(gb, gb->length + len);
  memcpy(&gb->ptr[gb->length], src, len);
  gb->length += len;
}
