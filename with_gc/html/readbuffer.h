#pragma once
#include "url.h"
#include "lineprop.h"
#include "enum_template.h"
#include "form.h"
#include "anchorlist.h"
#include "Str.h"
#include "anchor.h"
#include "readbuffer_status.h"
#include "html_command.h"
#include <memory>
#include <string_view>

extern int squeezeBlankLine;

#define N_GRAPH_SYMBOL 32
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)

#define N_SYMBOL (N_GRAPH_SYMBOL + 14)

extern bool pseudoInlines;
extern bool ignore_null_img_alt;
#define DEFAULT_PIXEL_PER_CHAR 8.0 /* arbitrary */
#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0
extern int pixel_per_char_i;
extern bool displayLinkNumber;
extern bool DisableCenter;
extern int IndentIncr;
#define INDENT_INCR IndentIncr
extern bool DisplayBorders;

enum DisplayInsDelType {
  DISPLAY_INS_DEL_SIMPLE = 0,
  DISPLAY_INS_DEL_NORMAL = 1,
  DISPLAY_INS_DEL_FONTIFY = 2,
};

extern int displayInsDel;
extern bool view_unseenobject;
extern bool MetaRefresh;

#define RB_STACK_SIZE 10
#define TAG_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define FONTSTAT_MAX 127

enum ReadBufferFlags {
  RB_NONE = 0,
  RB_PRE = 0x01,
  RB_SCRIPT = 0x02,
  RB_STYLE = 0x04,
  RB_PLAIN = 0x08,
  RB_LEFT = 0x10,
  RB_CENTER = 0x20,
  RB_RIGHT = 0x40,
  RB_ALIGN = (RB_LEFT | RB_CENTER | RB_RIGHT),
  RB_NOBR = 0x80,
  RB_P = 0x100,
  RB_PRE_INT = 0x200,
  RB_IN_DT = 0x400,
  RB_INTXTA = 0x800,
  RB_INSELECT = 0x1000,
  RB_IGNORE_P = 0x2000,
  RB_TITLE = 0x4000,
  RB_NFLUSHED = 0x8000,
  RB_NOFRAMES = 0x10000,
  RB_INTABLE = 0x20000,
  RB_PREMODE =
      (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_INTXTA),
  RB_SPECIAL =
      (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_NOBR),
  RB_PLAIN_PRE = 0x40000,
  RB_FILL = 0x80000,
  RB_DEL = 0x100000,
  RB_S = 0x200000,
  RB_HTML5 = 0x400000,
};
ENUM_OP_INSTANCE(ReadBufferFlags);

#define ST_IS_REAL_TAG(s)                                                      \
  ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE)

/* is this '<' really means the beginning of a tag? */
#define REALLY_THE_BEGINNING_OF_A_TAG(p)                                       \
  (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' ||              \
   p[1] == '\0' || p[1] == '_')

struct FontStat {
  char in_bold = 0;
  char in_under = 0;
  char in_italic = 0;
  char in_strike = 0;
  char in_ins = 0;
  char in_stand = 0;
};

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
  int pos = 0;
  int len = 0;
  int tlen = 0;
  ReadBufferFlags flag = {};
  Str *img_alt = nullptr;
  struct input_alt_attr input_alt = {};
  FontStat fontstat = {};
  short nobr_level = 0;
  Lineprop prev_ctype = 0;
  char init_flag = 1;
  short top_margin = 0;
  short bottom_margin = 0;
};

struct cmdtable {
  const char *cmdname;
  HtmlCommand cmd;
};

struct link_stack {
  int cmd = 0;
  short offset = 0;
  short pos = 0;
  link_stack *next = nullptr;
};

struct readbuffer {
  Str *line;
  Lineprop cprop = 0;
  short pos = 0;
  Str *prevchar;
  ReadBufferFlags flag = {};
  ReadBufferFlags flag_stack[RB_STACK_SIZE];
  int flag_sp = 0;
  ReadBufferStatus status = {};
  unsigned char end_tag;
  unsigned char q_level = 0;
  short table_level = -1;
  short nobr_level = 0;
  Anchor anchor = {};
  Str *img_alt = 0;
  struct input_alt_attr input_alt = {};
  FontStat fontstat = {};
  FontStat fontstat_stack[FONT_STACK_SIZE];
  int fontstat_sp = 0;
  Lineprop prev_ctype;
  Breakpoint bp;
  struct cmdtable *tag_stack[TAG_STACK_SIZE];
  int tag_sp = 0;
  short top_margin = 0;
  short bottom_margin = 0;
  struct link_stack *link_stack = nullptr;

  readbuffer();

  ReadBufferFlags RB_GET_ALIGN() const { return (this->flag & RB_ALIGN); }

  void RB_SET_ALIGN(ReadBufferFlags align) {
    this->flag &= ~RB_ALIGN;
    this->flag |= (align);
  }

  void RB_SAVE_FLAG() {
    if (this->flag_sp < RB_STACK_SIZE)
      this->flag_stack[this->flag_sp++] = RB_GET_ALIGN();
  }

  void RB_RESTORE_FLAG() {
    if (this->flag_sp > 0)
      RB_SET_ALIGN(this->flag_stack[--this->flag_sp]);
  }

  void set_breakpoint(int tag_length);
  void back_to_breakpoint() {
    this->flag = this->bp.flag;
    this->img_alt = this->bp.img_alt;
    this->input_alt = this->bp.input_alt;
    this->fontstat = this->bp.fontstat;
    this->prev_ctype = this->bp.prev_ctype;
    this->pos = this->bp.pos;
    this->top_margin = this->bp.top_margin;
    this->bottom_margin = this->bp.bottom_margin;
    if (this->flag & RB_NOBR) {
      this->nobr_level = this->bp.nobr_level;
    }
  }

  void append_tags();
  void push_tag(const char *cmdname, HtmlCommand cmd);
  void push_nchars(int width, const char *str, int len, Lineprop mode);

  // link
  void push_link(HtmlCommand cmd, int offset, int pos);
  char *has_hidden_link(HtmlCommand cmd) const;
  void passthrough(char *str, int back);
};

Str *romanNumeral(int n);
Str *romanAlphabet(int n);
// extern int next_status(char c, int *status);
int next_status(char c, ReadBufferStatus *status);
int read_token(Str *buf, const char **instr, ReadBufferStatus *status, int pre,
               int append);

struct LineLayout;
struct HttpResponse;
void loadHTMLstream(const std::shared_ptr<LineLayout> &layout, int width,
                    const Url &currentUrl, std::string_view body,
                    bool internal = false);
void loadBuffer(const std::shared_ptr<LineLayout> &layout, HttpResponse *res,
                std::string_view body);

enum CleanupMode {
  RAW_MODE = 0,
  PAGER_MODE = 1,
  HTML_MODE = 2,
  HEADER_MODE = 3,
};
std::string cleanup_line(std::string_view s, CleanupMode mode);
inline void cleanup_line(Str *s, CleanupMode mode) {
  auto tmp = cleanup_line(s->ptr, mode);
  Strclear(s);
  Strcat(s, tmp);
}

extern int is_boundary(unsigned char *, unsigned char *);

struct Buffer;
// std::shared_ptr<Buffer>
// loadHTMLString(int width, const Url &currentUrl, std::string_view html,
//                const std::shared_ptr<AnchorList<FormAnchor>> &forms = {});
std::shared_ptr<Buffer> getshell(const char *cmd);
