#pragma once
#include <stddef.h>

struct growbuf {
  char *ptr;
  int length;
  int area_size;
  void *(*realloc_proc)(void *, size_t);
  void (*free_proc)(void *);
};

struct Str;
Str *decodeB(char **ww);
void decodeB_to_growbuf(struct growbuf *gb, char **ww);
Str *decodeQ(char **ww);
Str *decodeQP(char **ww);
void decodeQP_to_growbuf(struct growbuf *gb, char **ww);
Str *decodeU(char **ww);
void decodeU_to_growbuf(struct growbuf *gb, char **ww);
Str *decodeWord0(char **ow);
Str *decodeMIME0(Str *orgstr);
#define decodeWord(ow, charset) decodeWord0(ow)
#define decodeMIME(orgstr, charset) decodeMIME0(orgstr)
