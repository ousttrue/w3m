#pragma once
#include "utf8.h"
#include "anchor.h"
#include "htmlcommand.h"

#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)

extern bool pseudoInlines;
extern bool ignore_null_img_alt;
#define DEFAULT_PIXEL_PER_CHAR 8.0 /* arbitrary */
#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0
extern double pixel_per_char;
extern int pixel_per_char_i;
extern int displayLinkNumber;
extern bool DisableCenter;
extern int Tabstop;
extern int IndentIncr;
#define INDENT_INCR IndentIncr
extern bool DisplayBorders;
#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2
extern int displayInsDel;
extern bool view_unseenobject;
extern bool MetaRefresh;

#define RB_STACK_SIZE 10
#define TAG_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define FONTSTAT_SIZE 7
#define FONTSTAT_MAX 127

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

#ifdef FORMAT_NICE
#define RB_FILL 0x80000
#endif /* FORMAT_NICE */
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

/* state of token scanning finite state machine */
#define R_ST_NORMAL 0  /* normal */
#define R_ST_TAG0 1    /* within tag, just after < */
#define R_ST_TAG 2     /* within tag */
#define R_ST_QUOTE 3   /* within single quote */
#define R_ST_DQUOTE 4  /* within double quote */
#define R_ST_EQL 5     /* = */
#define R_ST_AMP 6     /* within ampersand quote */
#define R_ST_EOL 7     /* end of file */
#define R_ST_CMNT1 8   /* <!  */
#define R_ST_CMNT2 9   /* <!- */
#define R_ST_CMNT 10   /* within comment */
#define R_ST_NCMNT1 11 /* comment - */
#define R_ST_NCMNT2 12 /* comment -- */
#define R_ST_NCMNT3 13 /* comment -- space */
#define R_ST_IRRTAG 14 /* within irregular tag */
#define R_ST_VALUE 15  /* within tag attribule value */

#define ST_IS_REAL_TAG(s)                                                      \
  ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE)

/* is this '<' really means the beginning of a tag? */
#define REALLY_THE_BEGINNING_OF_A_TAG(p)                                       \
  (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' ||              \
   p[1] == '\0' || p[1] == '_')

#define in_bold fontstat[0]
#define in_under fontstat[1]
#define in_italic fontstat[2]
#define in_strike fontstat[3]
#define in_ins fontstat[4]
#define in_stand fontstat[5]

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

struct cmdtable {
  const char *cmdname;
  int cmd;
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
};

void flushline(html_feed_environ *h_env, readbuffer *obuf, int indent,
               int force, int width);
void do_blankline(html_feed_environ *h_env, readbuffer *obuf, int indent,
                  int indent_incr, int width);
void purgeline(html_feed_environ *h_env);
void save_fonteffect(html_feed_environ *h_env, readbuffer *obuf);
void restore_fonteffect(html_feed_environ *h_env, readbuffer *obuf);
void push_render_image(Str *str, int width, int limit,
                       html_feed_environ *h_env);
int HTMLtagproc1(HtmlTag *tag, struct html_feed_environ *h_env);
void HTMLlineproc0(const char *istr, struct html_feed_environ *h_env,
                   int internal);
void init_henv(struct html_feed_environ *, struct readbuffer *,
               struct environment *, int, TextLineList *, int, int);
void completeHTMLstream(struct html_feed_environ *, struct readbuffer *);

Str *romanNumeral(int n);
Str *romanAlphabet(int n);
// extern int next_status(char c, int *status);
int next_status(char c, int *status);
int read_token(Str *buf, const char **instr, int *status, int pre, int append);

struct UrlStream;
struct Buffer;
void loadHTMLstream(UrlStream *f, Buffer *newBuf, FILE *src, int internal);
Buffer *loadHTMLBuffer(UrlStream *f, Buffer *newBuf);
Buffer *loadHTMLString(Str *page);

enum CleanupMode {
  RAW_MODE = 0,
  PAGER_MODE = 1,
  HTML_MODE = 2,
  HEADER_MODE = 3,
};
Str *convertLine0(Str *line, CleanupMode mode);

Str *getLinkNumberStr(int correction);
void cleanup_line(Str *s, CleanupMode mode);
const char *remove_space(const char *str);
struct Buffer;
extern int currentLn(Buffer *buf);
extern int is_boundary(unsigned char *, unsigned char *);
