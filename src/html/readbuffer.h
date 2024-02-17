#pragma once
#include "utf8.h"
#include "anchor.h"
#include "html_command.h"
#include <memory>

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
  int hseq = 0;
  int fid = -1;
  int in = 0;
  Str *type = nullptr;
  Str *name = nullptr;
  Str *value = nullptr;
};

struct Breakpoint {
  int pos;
  int len;
  int tlen;
  long flag;
  Anchor anchor;
  Str *img_alt;
  struct input_alt_attr input_alt = {};
  char fontstat[FONTSTAT_SIZE];
  short nobr_level;
  Lineprop prev_ctype;
  char init_flag = 1;
  short top_margin;
  short bottom_margin;
};

struct cmdtable {
  const char *cmdname;
  int cmd;
};

struct readbuffer {
  Str *line;
  Lineprop cprop = 0;
  short pos = 0;
  Str *prevchar;
  long flag;
  long flag_stack[RB_STACK_SIZE];
  int flag_sp = 0;
  int status;
  unsigned char end_tag;
  unsigned char q_level = 0;
  short table_level = -1;
  short nobr_level = 0;
  Anchor anchor = {0};
  Str *img_alt = 0;
  struct input_alt_attr input_alt = {};
  char fontstat[FONTSTAT_SIZE];
  char fontstat_stack[FONT_STACK_SIZE][FONTSTAT_SIZE];
  int fontstat_sp = 0;
  Lineprop prev_ctype;
  Breakpoint bp;
  struct cmdtable *tag_stack[TAG_STACK_SIZE];
  int tag_sp = 0;
  short top_margin = 0;
  short bottom_margin = 0;

  readbuffer();
  void set_breakpoint(int tag_length);
  void back_to_breakpoint() {
    this->flag = this->bp.flag;
    this->anchor = this->bp.anchor;
    this->img_alt = this->bp.img_alt;
    this->input_alt = this->bp.input_alt;
    this->in_bold = this->bp.in_bold;
    this->in_italic = this->bp.in_italic;
    this->in_under = this->bp.in_under;
    this->in_strike = this->bp.in_strike;
    this->in_ins = this->bp.in_ins;
    this->prev_ctype = this->bp.prev_ctype;
    this->pos = this->bp.pos;
    this->top_margin = this->bp.top_margin;
    this->bottom_margin = this->bp.bottom_margin;
    if (this->flag & RB_NOBR) {
      this->nobr_level = this->bp.nobr_level;
    }
  }
};

struct TextLineList;

Str *romanNumeral(int n);
Str *romanAlphabet(int n);
// extern int next_status(char c, int *status);
int next_status(char c, int *status);
int read_token(Str *buf, const char **instr, int *status, int pre, int append);

struct UrlStream;
struct HttpResponse;
struct LineLayout;
void loadHTMLstream(HttpResponse *res, LineLayout *layout,
                    bool internal = false);

enum CleanupMode {
  RAW_MODE = 0,
  PAGER_MODE = 1,
  HTML_MODE = 2,
  HEADER_MODE = 3,
};
void cleanup_line(Str *s, CleanupMode mode);

extern int is_boundary(unsigned char *, unsigned char *);

void loadBuffer(HttpResponse *res, LineLayout *layout);
struct Buffer;
std::shared_ptr<Buffer> loadHTMLString(Str *page);
std::shared_ptr<Buffer> getshell(const char *cmd);

const char *html_quote(const char *str);
const char *html_unquote(const char *str);
int getescapechar(const char **s);
const char *getescapecmd(const char **s);
