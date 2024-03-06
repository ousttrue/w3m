#pragma once
#include <stdio.h>
#include <vector>
#include "html_command.h"
#include "readbuffer.h"
#include "textline.h"
#include "generallist.h"

struct Str;
struct TextLineList;
struct readbuffer;

struct environment {
  HtmlCommand env;
  int type;
  int count;
  char indent;
};

struct html_feed_environ {
  readbuffer obuf;
  GeneralList<TextLine> *buf;
  FILE *f = nullptr;
  Str *tagbuf;
  int limit;
  int maxlimit = 0;

  std::vector<environment> envs;
  int envc = 0;

  int envc_real = 0;
  const char *title = nullptr;
  int blank_lines = 0;

  html_feed_environ(int envc, int, int, GeneralList<TextLine> *_buf = nullptr);
  void purgeline();
};

int sloppy_parse_line(char **str);
