#pragma once
#include <stddef.h>
#include <gc_cpp.h>
#include "lineprop.h"

#define LINELEN 256 /* Initial line length */

struct Line : public gc_cleanup {
  long linenumber = 0;  /* on buffer */
  long real_linenumber; /* on file */
  Line *next = nullptr;
  Line *prev = nullptr;

  char *lineBuf = {};
  Lineprop *propBuf = 0;
  int size = 0;
  // column width. cache
  int width = -1;

  // line break
  int len = 0;
  int bpos = 0;
  int bwidth = 0;

  Line(int n, Line *prevl = nullptr);
  Line(int linenumber, Line *prevl, char *line, Lineprop *prop, int byteLen,
       int realLinenumber);
  ~Line() {}

  Line(const Line &) = delete;
  Line &operator=(const Line &) = delete;

  // byte pos to column
  int bytePosToColumn(int pos) const;
};
