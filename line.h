#pragma once

typedef unsigned short Lineprop;

typedef struct Line {
  char *lineBuf;
  Lineprop *propBuf;
  struct Line *next;
  struct Line *prev;
  int len;
  int width;
  long linenumber;      /* on buffer */
  long real_linenumber; /* on file */
  unsigned short usrflags;
  int size;
  int bpos;
  int bwidth;
} Line;
