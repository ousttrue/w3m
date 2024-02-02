#pragma once
#include <stddef.h>
#include <gc_cpp.h>
#include "lineprop.h"

#define LINELEN 256 /* Initial line length */

struct Line : public gc_cleanup {
  long linenumber; /* on buffer */
  Line *next = nullptr;
  Line *prev;

  char *lineBuf = 0;
  Lineprop *propBuf = 0;
  int len = 0;

  int width = -1;
  long real_linenumber; /* on file */
  unsigned short usrflags;
  int size = 0;

  // line break
  int bpos = 0;
  int bwidth = 0;

  Line(int n, Line *prevl = nullptr) : linenumber(n), prev(prevl) {
    if (prev) {
      prev->next = this;
    }
  }
  ~Line() {}

  Line(const Line &) = delete;
  Line &operator=(const Line &) = delete;

  // byte pos to column
  int bytePosToColumn(int pos) const;
};

