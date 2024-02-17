#pragma once
#include <stdio.h>

struct Str;
struct TextLineList;
struct html_feed_environ {
  struct readbuffer *obuf;
  TextLineList *buf;
  FILE *f;
  Str *tagbuf;
  int limit;
  int maxlimit;
  struct environment *envs;
  int nenv;
  int envc;
  int envc_real;
  const char *title;
  int blank_lines;

  void purgeline();
};

int sloppy_parse_line(char **str);
