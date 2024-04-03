#pragma once
#include "lineprop.h"
#include "enum_template.h"
#include "generallist.h"
#include "matrix.h"
#include "html_readbuffer_flags.h"

#include <stdint.h>
#include <memory>

#define MAX_TABLE 20 /* maximum nest level of table */
#define MAX_TABLE_N_LIMIT 2000
#define MAX_TABLE_N 20 /* maximum number of table in same level */

#define MAXROW_LIMIT 32767
#define MAXROW 50
#define MAXCOL 256

enum HtmlTableBorderMode {
  BORDER_NONE = 0,
  BORDER_THIN = 1,
  BORDER_THICK = 2,
  BORDER_NOWIN = 3,
};

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))

/* flag */
enum TableFlags {
  TBL_IN_ROW = 1,
  TBL_EXPAND_OK = 2,
  TBL_IN_COL = 4,
};
ENUM_OP_INSTANCE(TableFlags);

#define MAXCELL 20
#define MAXROWCELL 1000
struct table_cell {
  short col[MAXCELL];
  short colspan[MAXCELL];
  short index[MAXCELL];
  short maxcell;
  short icell;
  short eindex[MAXCELL];
  short necell;
  short width[MAXCELL];
  short minimum_width[MAXCELL];
  short fixed_width[MAXCELL];
};

struct Str;
struct table_in {
  std::shared_ptr<struct table> ptr;
  short col;
  short row;
  short cell;
  short indent;
  std::shared_ptr<GeneralList> buf;
};

struct table_linfo {
  Lineprop prev_ctype;
  signed char prev_spaces;
  std::string prevchar;
  short length;
};

struct table_mode {
  ReadBufferFlags pre_mode;
  char indent_level;
  char caption;
  short nobr_offset;
  char nobr_level;
  short anchor_offset;
  HtmlCommand end_tag;
};

enum TableWidthFlags {
  CHECK_NONE = 0,
  CHECK_MINIMUM = 1,
  CHECK_FIXED = 2,
};

#define NOWRAP
enum table_attr : uint16_t {
  HTT_X = 1,
  HTT_Y = 2,
  HTT_ALIGN = 0x30,
  HTT_LEFT = 0x00,
  HTT_CENTER = 0x10,
  HTT_RIGHT = 0x20,
  HTT_TRSET = 0x40,
  HTT_VALIGN = 0x700,
  HTT_TOP = 0x100,
  HTT_MIDDLE = 0x200,
  HTT_BOTTOM = 0x400,
  HTT_VTRSET = 0x800,
#ifdef NOWRAP
  HTT_NOWRAP = 4,
#endif /* NOWRAP */
};
ENUM_OP_INSTANCE(table_attr);

struct table {
private:
public:
  int total_width;
  int row;
  int col;
  short ntable;
  int vspace;

  struct tableimpl *_impl;

private:
  table(int cols);

public:
  ~table();
  table(const table &) = delete;
  table &operator=(const table &) = delete;

private:
  std::vector<int> tabheight;
  std::vector<table_in> tables;
  short tables_size;
  std::shared_ptr<GeneralList> suspended_data;
  /* use for counting skipped spaces */
  struct table_linfo linfo;
  Matrix matrix;
  Vector vector;
  int sloppy_width;

public:
  static std::shared_ptr<table> newTable(int cols);
  static std::shared_ptr<table> begin_table(HtmlTableBorderMode border,
                                            int spacing, int padding,
                                            int vspace, int cols, int width);
  int feed_table(class html_feed_environ *parser, std::string line,
                 struct table_mode *mode, int width, int internal);
  void end_table();
  void pushTable(const std::shared_ptr<table> &);
  void renderTable(html_feed_environ *parser, int max_width);

private:
  int feed_table_tag(html_feed_environ *parser, const std::string &line,
                     struct table_mode *mode, int width,
                     const std::shared_ptr<class HtmlTag> &tag);

  void pushdata(int row, int col, const std::string &data);
  void check_rowcol(table_mode *mode);
  void check_row(int row);
  void table_close_anchor0(struct table_mode *mode);
  void check_minimum0(int min);
  void addcontentssize(int width);
  void setwidth(struct table_mode *mode);
  int setwidth0(struct table_mode *mode);
  void clearcontentssize(struct table_mode *mode);
  void begin_cell(struct table_mode *mode);
  int skip_space(const char *line, struct table_linfo *linfo, int checkminimum);
  void feed_table_block_tag(const std::string &line, struct table_mode *mode,
                            int indent, int cmd);
  void feed_table_inline_tag(const std::string &line, struct table_mode *mode,
                             int width);
  void suspend_or_pushdata(const std::string &line);
  void table_close_textarea(html_feed_environ *parser, struct table_mode *mode,
                            int width);
  void feed_table1(html_feed_environ *parser, const std::string &tok,
                   struct table_mode *mode, int width);
  void table_close_select(html_feed_environ *parser, struct table_mode *mode,
                          int width);
  void print_item(int row, int col, int width, std::ostream &buf);
  void print_sep(int row, int type, int maxcol, std::ostream &buf);
  void make_caption(html_feed_environ *parser);
  void renderCoTable(html_feed_environ *parser, int maxlimit);
  void check_table_height();
  void set_table_width(short *newwidth, int maxwidth);
  int check_table_width(double *newwidth, struct Matrix *minv, int itr);
  int check_compressible_cell(struct Matrix *minv, double *newwidth,
                              double *swidth, short *cwidth, double totalwidth,
                              double *Sxx, int icol, int icell, double sxx,
                              int corr);
  void set_integered_width(double *dwidth, short *iwidth);
  void check_maximum_width();
  void do_refill(html_feed_environ *parser, int row, int col, int maxlimit);
  int get_spec_cell_width(int row, int col);
  int correct_table_matrix(int col, int cspan, int a, double b);
  void correct_table_matrix2(int col, int cspan, double s, double b);
  void set_table_matrix(int width);
  void set_table_matrix0(int maxwidth);
  void check_relative_width(int maxwidth);
  void correct_table_matrix3(int col, char *flags, double s, double b);
  void correct_table_matrix4(int col, int cspan, char *flags, double s,
                             double b);
};

#define TBLM_PRE RB_PRE
#define TBLM_SCRIPT RB_SCRIPT
#define TBLM_STYLE RB_STYLE
#define TBLM_PLAIN RB_PLAIN
#define TBLM_NOBR RB_NOBR
#define TBLM_PRE_INT RB_PRE_INT
#define TBLM_INTXTA RB_INTXTA
#define TBLM_INSELECT RB_INSELECT
#define TBLM_PREMODE                                                           \
  (TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN |           \
   TBLM_INTXTA)
#define TBLM_SPECIAL                                                           \
  (TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_NOBR)
#define TBLM_DEL RB_DEL
#define TBLM_S RB_S

extern int visible_length(const char *str);

extern void initRenderTable(void);
extern int minimum_length(char *line);

struct TableStatus {
  std::shared_ptr<table> tbl;
  table_mode *tbl_mode = nullptr;
  int tbl_width = 0;

  ReadBufferFlags pre_mode(html_feed_environ *h_env);
  HtmlCommand end_tag(html_feed_environ *h_env);
  bool is_active(html_feed_environ *h_env);
  int feed(html_feed_environ *parser, const std::string &str, bool internal);
};
