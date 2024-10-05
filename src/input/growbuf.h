#pragma once
#include "text/Str.h"

struct growbuf {
  char *ptr;
  int length;
  int area_size;
  void *(*realloc_proc)(void *, size_t);
  void (*free_proc)(void *);
};

extern void growbuf_init(struct growbuf *gb);
extern void growbuf_init_without_GC(struct growbuf *gb);
extern void growbuf_clear(struct growbuf *gb);
extern Str growbuf_to_Str(struct growbuf *gb);
extern void growbuf_reserve(struct growbuf *gb, int leastarea);
extern void growbuf_append(struct growbuf *gb, const unsigned char *src,
                           int len);
#define GROWBUF_ADD_CHAR(gb, ch)                                               \
  ((((gb)->length >= (gb)->area_size) ? growbuf_reserve(gb, (gb)->length + 1)  \
                                      : (void)0),                              \
   (void)((gb)->ptr[(gb)->length++] = (ch)))

extern void decodeB_to_growbuf(struct growbuf *gb, char **ww);
extern void decodeQP_to_growbuf(struct growbuf *gb, char **ww);
extern void decodeU_to_growbuf(struct growbuf *gb, char **ww);
