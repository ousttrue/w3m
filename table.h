/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_TABLE_H
#define W3M_TABLE_H

#include "Str.h"
#include "buffer.h"
#include "file.h"
#include "textlist.h"

#include "matrix.h"

#define MAX_TABLE 20		/* maximum nest level of table */
#define MAX_TABLE_N_LIMIT 2000
#define MAX_TABLE_N 20		/* maximum number of table in same level */

#define MAXROW_LIMIT 32767
#define MAXROW 50
#define MAXCOL 256

#define BORDER_NONE 0
#define BORDER_THIN 1
#define BORDER_THICK 2
#define BORDER_NOWIN 3

typedef unsigned short table_attr;

/* flag */
#define TBL_IN_ROW     1
#define TBL_EXPAND_OK  2
#define TBL_IN_COL     4

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
    struct table *ptr;
    short col;
    short row;
    short cell;
    short indent;
    TextLineList *buf;
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
#ifdef TABLE_EXPAND
    int real_width;
#endif				/* TABLE_EXPAND */
    Str caption;
    Str id;
    GeneralList ***tabdata;
    table_attr **tabattr;
    table_attr trattr;
    Str **tabidvalue;
    Str *tridvalue;
    short tabwidth[MAXCOL];
    short minimum_width[MAXCOL];
    short fixed_width[MAXCOL];
    struct table_cell cell;
    int *tabheight;
    struct table_in *tables;
    short ntable;
    short tables_size;
    TextList *suspended_data;
    /* use for counting skipped spaces */
    struct table_linfo linfo;
    MAT *matrix;
    VEC *vector;
    int sloppy_width;
};

#define TBLM_PRE	RB_PRE
#define TBLM_SCRIPT	RB_SCRIPT
#define TBLM_STYLE	RB_STYLE
#define TBLM_PLAIN	RB_PLAIN
#define TBLM_NOBR	RB_NOBR
#define TBLM_PRE_INT	RB_PRE_INT
#define TBLM_INTXTA	RB_INTXTA
#define TBLM_INSELECT	RB_INSELECT
#define TBLM_PREMODE	(TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_INTXTA)
#define TBLM_SPECIAL	(TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_NOBR)
#define TBLM_DEL	RB_DEL
#define TBLM_S		RB_S
#define TBLM_ANCHOR	0x1000000

struct table_mode {
    unsigned int pre_mode;
    char indent_level;
    char caption;
    short nobr_offset;
    char nobr_level;
    short anchor_offset;
    unsigned char end_tag;
};

int feed_table(struct table *tbl, char *line, struct table_mode *mode, int width, int internal);
int visible_length(const char *str);
struct table *begin_table(int border, int spacing, int padding, int vspace);
void align(TextLine *lbuf, int width, int mode);
void check_rowcol(struct table *tbl, struct table_mode *mode);
void end_table(struct table *tbl);
void initRenderTable(void);
void pushTable(struct table *, struct table *);
void renderTable(struct table *t, int max_width, struct html_feed_environ *h_env);

#endif
