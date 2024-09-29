#pragma once
#include "fm.h"
#include "Str.h"
#include "anchor.h"
#include "line.h"

#define RB_PRE 0x01
#define RB_SCRIPT 0x02
#define RB_STYLE 0x04
#define RB_PLAIN 0x08
#define RB_LEFT 0x10
#define RB_CENTER 0x20
#define RB_RIGHT 0x40
#define RB_ALIGN (RB_LEFT | RB_CENTER | RB_RIGHT)
#define RB_NOBR 0x80
#define RB_P 0x100
#define RB_PRE_INT 0x200
#define RB_IN_DT 0x400
#define RB_INTXTA 0x800
#define RB_INSELECT 0x1000
#define RB_IGNORE_P 0x2000
#define RB_TITLE 0x4000
#define RB_NFLUSHED 0x8000
#define RB_NOFRAMES 0x10000
#define RB_INTABLE 0x20000
#define RB_PREMODE                                                             \
  (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_INTXTA)
#define RB_SPECIAL                                                             \
  (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_NOBR)
#define RB_PLAIN_PRE 0x40000

#define RB_FILL 0x80000
#define RB_DEL 0x100000
#define RB_S 0x200000
#define RB_HTML5 0x400000

#define RB_GET_ALIGN(obuf) ((obuf)->flag & RB_ALIGN)
#define RB_SET_ALIGN(obuf, align)                                              \
  do {                                                                         \
    (obuf)->flag &= ~RB_ALIGN;                                                 \
    (obuf)->flag |= (align);                                                   \
  } while (0)
#define RB_SAVE_FLAG(obuf)                                                     \
  {                                                                            \
    if ((obuf)->flag_sp < RB_STACK_SIZE)                                       \
      (obuf)->flag_stack[(obuf)->flag_sp++] = RB_GET_ALIGN(obuf);              \
  }
#define RB_RESTORE_FLAG(obuf)                                                  \
  {                                                                            \
    if ((obuf)->flag_sp > 0)                                                   \
      RB_SET_ALIGN(obuf, (obuf)->flag_stack[--(obuf)->flag_sp]);               \
  }


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
  struct Anchor anchor;
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
  struct Anchor anchor;
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

struct html_feed_environ {
  struct readbuffer *obuf;
  struct TextLineList *buf;
  FILE *f;
  Str tagbuf;
  int limit;
  int maxlimit;
  struct environment *envs;
  int nenv;
  int envc;
  int envc_real;
  char *title;
  int blank_lines;
};

void push_link(int cmd, int offset, int pos);

int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env);
void HTMLlineproc2(struct Buffer *buf, struct TextLineList *tl);
void HTMLlineproc0(char *istr, struct html_feed_environ *h_env, int internal);
#define HTMLlineproc1(x, y) HTMLlineproc0(x, y, true)
struct Buffer *loadHTMLBuffer(struct URLFile *f, struct Buffer *newBuf);
void init_henv(struct html_feed_environ *, struct readbuffer *,
               struct environment *, int, struct TextLineList *, int, int);
void completeHTMLstream(struct html_feed_environ *, struct readbuffer *);
void loadHTMLstream(struct URLFile *f, struct Buffer *newBuf, FILE *src,
                    int internal);
struct Buffer *loadHTMLString(Str page);
struct Buffer *loadSomething(struct URLFile *f,
                             struct Buffer *(*loadproc)(struct URLFile *,
                                                        struct Buffer *),
                             struct Buffer *defaultbuf);
struct Buffer *loadBuffer(struct URLFile *uf, struct Buffer *newBuf);
