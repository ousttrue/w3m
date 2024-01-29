#pragma once
#include "utf8.h"
#include "anchor.h"

#define RB_STACK_SIZE 10
#define TAG_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define FONTSTAT_SIZE 7
#define FONTSTAT_MAX 127

struct Str;
struct input_alt_attr {
  int hseq;
  int fid;
  int in;
  Str *type;
  Str *name;
  Str *value;
};

struct Breakpoint {
  int pos;
  int len;
  int tlen;
  long flag;
  Anchor anchor;
  Str *img_alt;
  struct input_alt_attr input_alt;
  char fontstat[FONTSTAT_SIZE];
  short nobr_level;
  Lineprop prev_ctype;
  char init_flag;
  short top_margin;
  short bottom_margin;
};

struct readbuffer {
  Str *line;
  Lineprop cprop;
  short pos;
  Str *prevchar;
  long flag;
  long flag_stack[RB_STACK_SIZE];
  int flag_sp;
  int status;
  unsigned char end_tag;
  unsigned char q_level;
  short table_level;
  short nobr_level;
  Anchor anchor;
  Str *img_alt;
  struct input_alt_attr input_alt;
  char fontstat[FONTSTAT_SIZE];
  char fontstat_stack[FONT_STACK_SIZE][FONTSTAT_SIZE];
  int fontstat_sp;
  Lineprop prev_ctype;
  Breakpoint bp;
  struct cmdtable *tag_stack[TAG_STACK_SIZE];
  int tag_sp;
  short top_margin;
  short bottom_margin;
};
