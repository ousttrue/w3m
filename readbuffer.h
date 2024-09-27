#pragma once
#include "fm.h"

struct input_alt_attr {
  int hseq;
  int fid;
  int in;
  Str type, name, value;
};

typedef struct {
  int pos;
  int len;
  int tlen;
  long flag;
  Anchor anchor;
  Str img_alt;
  struct input_alt_attr input_alt;
  char fontstat[FONTSTAT_SIZE];
  short nobr_level;
  Lineprop prev_ctype;
  char init_flag;
  short top_margin;
  short bottom_margin;
} Breakpoint;

struct readbuffer {
  Str line;
  Lineprop cprop;
  short pos;
  Str prevchar;
  long flag;
  long flag_stack[RB_STACK_SIZE];
  int flag_sp;
  int status;
  unsigned char end_tag;
  unsigned char q_level;
  short table_level;
  short nobr_level;
  Anchor anchor;
  Str img_alt;
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

void push_link(int cmd, int offset, int pos);

int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env);
void HTMLlineproc2(struct Buffer *buf, TextLineList *tl);
void HTMLlineproc0(char *istr, struct html_feed_environ *h_env, int internal);
#define HTMLlineproc1(x, y) HTMLlineproc0(x, y, TRUE)
struct Buffer *loadHTMLBuffer(struct URLFile *f, struct Buffer *newBuf);
void init_henv(struct html_feed_environ *, struct readbuffer *,
               struct environment *, int, TextLineList *, int, int);
void completeHTMLstream(struct html_feed_environ *, struct readbuffer *);
void loadHTMLstream(struct URLFile *f, struct Buffer *newBuf, FILE *src,
                    int internal);
struct Buffer *loadHTMLString(Str page);
struct Buffer *loadSomething(struct URLFile *f,
                             struct Buffer *(*loadproc)(struct URLFile *,
                                                        struct Buffer *),
                             struct Buffer *defaultbuf);
struct Buffer *loadBuffer(struct URLFile *uf, struct Buffer *newBuf);
