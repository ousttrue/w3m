#pragma once
#include <stdio.h>
#include <vector>
#include "html_command.h"
#include "readbuffer.h"
#include "textline.h"
#include "generallist.h"

#define MAX_INDENT_LEVEL 10

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
  Str *tagbuf;
  int limit;
  int maxlimit = 0;

  std::vector<environment> envs;
  int envc = 0;

  int envc_real = 0;
  std::string title;
  int blank_lines = 0;

  html_feed_environ(int envc, int, int, GeneralList<TextLine> *_buf = nullptr);
  void purgeline();

  void POP_ENV() {
    if (this->envc_real-- < (int)this->envs.size()) {
      this->envc--;
    }
  }

  void PUSH_ENV_NOINDENT(HtmlCommand cmd) {
    if (++this->envc_real < (int)this->envs.size()) {
      ++this->envc;
      envs[this->envc].env = cmd;
      envs[this->envc].count = 0;
      envs[this->envc].indent = envs[this->envc - 1].indent;
    }
  }

  void PUSH_ENV(HtmlCommand cmd) {
    if (++this->envc_real < (int)this->envs.size()) {
      ++this->envc;
      envs[this->envc].env = cmd;
      envs[this->envc].count = 0;
      if (this->envc <= MAX_INDENT_LEVEL)
        envs[this->envc].indent = envs[this->envc - 1].indent + INDENT_INCR;
      else
        envs[this->envc].indent = envs[this->envc - 1].indent;
    }
  }
};

int sloppy_parse_line(char **str);
