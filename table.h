#pragma once
#include "Line.h"
#include "textlist.h"

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))

#if (defined(MESCHACH) && !defined(MATRIX))
#define MATRIX
#endif /* (defined(MESCHACH) && !defined(MATRIX)) */

#ifdef MESCHACH
#include <matrix2.h>
#else /* not MESCHACH */
#include "matrix.h"
#endif /* not MESCHACH */

#include <gcstr/Str.h>

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

typedef unsigned short table_attr;

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

struct table_in {
    struct table* ptr;
    short col;
    short row;
    short cell;
    short indent;
    TextLineList* buf;
};

struct table_linfo {
    Lineprop prev_ctype;
    signed char prev_spaces;
    Str prevchar;
    short length;
};

struct table {
    int row;
    int col;
    int maxrow;
    int maxcol;
    int max_rowsize;
    int border_mode;
    int total_width;
    int total_height;
    int tabcontentssize;
    int indent;
    int cellspacing;
    int cellpadding;
    int vcellpadding;
    int vspace;
    int flag;
    Str caption;
    Str id;
    GeneralList*** tabdata;
    table_attr** tabattr;
    table_attr trattr;
    Str** tabidvalue;
    Str* tridvalue;
    short tabwidth[MAXCOL];
    short minimum_width[MAXCOL];
    short fixed_width[MAXCOL];
    struct table_cell cell;
    int* tabheight;
    struct table_in* tables;
    short ntable;
    short tables_size;
    TextList* suspended_data;
    /* use for counting skipped spaces */
    struct table_linfo linfo;
    MAT* matrix;
    VEC* vector;
    int sloppy_width;
};

#define TBLM_PRE RB_PRE
#define TBLM_SCRIPT RB_SCRIPT
#define TBLM_STYLE RB_STYLE
#define TBLM_PLAIN RB_PLAIN
#define TBLM_NOBR RB_NOBR
#define TBLM_PRE_INT RB_PRE_INT
#define TBLM_INTXTA RB_INTXTA
#define TBLM_INSELECT RB_INSELECT
#define TBLM_PREMODE (TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_INTXTA)
#define TBLM_SPECIAL (TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_NOBR)
#define TBLM_DEL RB_DEL
#define TBLM_S RB_S
#define TBLM_ANCHOR 0x1000000

struct table_mode {
    unsigned int pre_mode;
    char indent_level;
    char caption;
    short nobr_offset;
    char nobr_level;
    short anchor_offset;
    unsigned char end_tag;
};

struct html_feed_environ;
void renderTable(struct table* t, int max_width,
    struct html_feed_environ* h_env);
void check_rowcol(struct table* tbl, struct table_mode* mode);
int feed_table(struct table* tbl, const char* line, struct table_mode* mode,
    int width, int internal);
void feed_table1(struct table* tbl, Str tok, struct table_mode* mode,
    int width);
void align(TextLine* lbuf, int width, int mode);
void print_item(struct table* t, int row, int col, int width, Str buf);
void print_sep(struct table* t, int row, int type, int maxcol, Str buf);
struct table* begin_table(int border, int spacing, int padding,
    int vspace);
void end_table(struct table* tbl);
void pushTable(struct table*, struct table*);
void initRenderTable(void);
