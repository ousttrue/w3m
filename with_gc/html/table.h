#pragma once
#include "lineprop.h"
#include "enum_template.h"
#include "generallist.h"
#include "readbuffer.h"
#include "matrix.h"

#include <stdint.h>
#include <memory>

#define MAX_TABLE 20 /* maximum nest level of table */
#define MAX_TABLE_N_LIMIT 2000
#define MAX_TABLE_N 20 /* maximum number of table in same level */

#define MAXROW_LIMIT 32767
#define MAXROW 50
#define MAXCOL 256

#define BORDER_NONE 0
#define BORDER_THIN 1
#define BORDER_THICK 2
#define BORDER_NOWIN 3

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))

/* flag */
#define TBL_IN_ROW 1
#define TBL_EXPAND_OK 2
#define TBL_IN_COL 4

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
  int total_width;
  int row;
  int col;
  short ntable;
  int vspace;

private:
  int maxrow;
  int maxcol;
  int max_rowsize;
  int border_mode;
  int total_height;
  int tabcontentssize;
  int indent;
  int cellspacing;
  int cellpadding;
  int vcellpadding;
  int flag;
  std::string caption;
  std::vector<std::vector<std::shared_ptr<GeneralList>>> tabdata;
  std::vector<std::vector<table_attr>> tabattr;
  table_attr trattr;
  short tabwidth[MAXCOL];
  short minimum_width[MAXCOL];
  short fixed_width[MAXCOL];
  struct table_cell cell;
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
  static std::shared_ptr<table> newTable();
  static std::shared_ptr<table> begin_table(int border, int spacing,
                                            int padding, int vspace);
  int feed_table(class HtmlParser *parser, const char *line,
                 struct table_mode *mode, int width, int internal);
  void end_table();
  void pushTable(const std::shared_ptr<table> &);
  void renderTable(HtmlParser *parser, int max_width,
                   struct html_feed_environ *h_env);

private:
  int feed_table_tag(class HtmlParser *parser, const char *line,
                     struct table_mode *mode, int width,
                     const std::shared_ptr<HtmlTag> &tag);

  int table_rule_width() const;
  int get_table_width(const short *orgwidth, const short *cellwidth,
                      TableWidthFlags flag) const;
  void check_minimum_width(short *tabwidth) const;
  int table_border_width() const;
  int minimum_table_width() const {
    return (this->get_table_width(this->minimum_width, this->cell.minimum_width,
                                  {}));
  }
  int maximum_table_width() const {
    return (
        this->get_table_width(this->tabwidth, this->cell.width, CHECK_FIXED));
  }
  int fixed_table_width() const {
    return (this->get_table_width(this->fixed_width, this->cell.fixed_width,
                                  CHECK_MINIMUM));
  }

  void pushdata(int row, int col, const char *data);
  void check_rowcol(table_mode *mode);
  void check_row(int row);
  int table_colspan(int row, int col) const {
    int i;
    for (i = col + 1; i <= this->maxcol && (this->tabattr[row][i] & HTT_X); i++)
      ;
    return i - col;
  }
  void table_close_anchor0(struct table_mode *mode);
  void check_minimum0(int min);
  void addcontentssize(int width);
  void setwidth(struct table_mode *mode);
  int setwidth0(struct table_mode *mode);
  void clearcontentssize(struct table_mode *mode);
  void begin_cell(struct table_mode *mode);
  int skip_space(const char *line, struct table_linfo *linfo, int checkminimum);
  void feed_table_block_tag(const char *line, struct table_mode *mode,
                            int indent, int cmd);
  void feed_table_inline_tag(const char *line, struct table_mode *mode,
                             int width);
  void suspend_or_pushdata(const char *line);
  void table_close_textarea(HtmlParser *parser, struct table_mode *mode,
                            int width);
  void feed_table1(HtmlParser *parser, const std::string &tok, struct table_mode *mode,
                   int width);
  void table_close_select(HtmlParser *parser, struct table_mode *mode,
                          int width);
  void print_item(int row, int col, int width, Str *buf);
  void print_sep(int row, int type, int maxcol, Str *buf);
  void make_caption(HtmlParser *parser, struct html_feed_environ *h_env);
  void renderCoTable(HtmlParser *parser, int maxlimit);
  void check_table_height();
  int table_rowspan(int row, int col);
  void set_table_width(short *newwidth, int maxwidth);
  int check_table_width(double *newwidth, struct Matrix *minv, int itr);
  int check_compressible_cell(struct Matrix *minv, double *newwidth,
                              double *swidth, short *cwidth, double totalwidth,
                              double *Sxx, int icol, int icell, double sxx,
                              int corr);
  void set_integered_width(double *dwidth, short *iwidth);
  void check_maximum_width();
  void do_refill(HtmlParser *parser, int row, int col, int maxlimit);
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

class HtmlParser;
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
  int feed(HtmlParser *parser, const std::string &str, bool internal);
};
