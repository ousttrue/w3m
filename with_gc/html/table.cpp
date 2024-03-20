/*
 * HTML table
 */
#include "readbuffer.h"
#include "app.h"
#include "quote.h"
#include "html_feed_env.h"
#include "symbol.h"
#include "html_command.h"
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "table.h"
#include "html.h"
#include "html_tag.h"
#include "Str.h"
#include "myctype.h"
#include "entity.h"
#include "proc.h"
#include "textline.h"
#include "generallist.h"
#include "matrix.h"
#include "utf8.h"

int symbol_width = 0;
double pixel_per_char = DEFAULT_PIXEL_PER_CHAR;

#define RULE_WIDTH symbol_width
#define RULE(mode, n) (((mode) == BORDER_THICK) ? ((n) + 16) : (n))
#define TK_VERTICALBAR(mode) RULE(mode, 5)

#define BORDERWIDTH 2
#define BORDERHEIGHT 1
#define NOBORDERWIDTH 1
#define NOBORDERHEIGHT 0

#define TAG_IS(s, tag, len)                                                    \
  (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */
#ifndef abs
#define abs(a) ((a) >= 0. ? (a) : -(a))
#endif /* not abs */

static double weight(int x) {

  if (x < App::instance().COLS())
    return (double)x;
  else
    return App::instance().COLS() *
           (log((double)x / App::instance().COLS()) + 1.);
}

static double weight2(int a) {
  return (double)a / App::instance().COLS() * 4 + 1.;
}

#define sigma_td(a) (0.5 * weight2(a))     /* <td width=...> */
#define sigma_td_nw(a) (32 * weight2(a))   /* <td ...> */
#define sigma_table(a) (0.25 * weight2(a)) /* <table width=...> */
#define sigma_table_nw(a) (2 * weight2(a)) /* <table...> */

static int bsearch_2short(short e1, short *ent1, short e2, short *ent2,
                          int base, short *indexarray, int nent) {
  int n = nent;
  int k = 0;

  int e = e1 * base + e2;
  while (n > 0) {
    int nn = n / 2;
    int idx = indexarray[k + nn];
    int ne = ent1[idx] * base + ent2[idx];
    if (ne == e) {
      k += nn;
      break;
    } else if (ne < e) {
      n -= nn + 1;
      k += nn + 1;
    } else {
      n = nn;
    }
  }
  return k;
}

static int bsearch_double(double e, double *ent, short *indexarray, int nent) {
  int n = nent;
  int k = 0;

  while (n > 0) {
    int nn = n / 2;
    int idx = indexarray[k + nn];
    double ne = ent[idx];
    if (ne == e) {
      k += nn;
      break;
    } else if (ne > e) {
      n -= nn + 1;
      k += nn + 1;
    } else {
      n = nn;
    }
  }
  return k;
}

static int ceil_at_intervals(int x, int step) {
  int mo = x % step;
  if (mo > 0)
    x += step - mo;
  else if (mo < 0)
    x -= mo;
  return x;
}

static int floor_at_intervals(int x, int step) {
  int mo = x % step;
  if (mo > 0)
    x -= mo;
  else if (mo < 0)
    x += step - mo;
  return x;
}

#define round(x) ((int)floor((x) + 0.5))

int table::table_rowspan(int row, int col) {
  int i;
  if (!this->tabattr[row])
    return 0;
  for (i = row + 1;
       i <= this->maxrow && this->tabattr[i] && (this->tabattr[i][col] & HTT_Y);
       i++)
    ;
  return i - row;
}

static int minimum_cellspacing(int border_mode) {
  switch (border_mode) {
  case BORDER_THIN:
  case BORDER_THICK:
  case BORDER_NOWIN:
    return RULE_WIDTH;
  case BORDER_NONE:
    return 1;
  default:
    /* not reached */
    return 0;
  }
}

int table::table_border_width() const {
  switch (this->border_mode) {
  case BORDER_THIN:
  case BORDER_THICK:
    return this->maxcol * this->cellspacing +
           2 * (RULE_WIDTH + this->cellpadding);
  case BORDER_NOWIN:
  case BORDER_NONE:
    return this->maxcol * this->cellspacing;
  default:
    /* not reached */
    return 0;
  }
}

table *table::newTable() {
  struct table *t;
  int i, j;

  t = (struct table *)New(struct table);
  t->max_rowsize = MAXROW;
  t->tabdata.resize(MAXROW);
  t->tabattr = (table_attr **)New_N(table_attr *, MAXROW);
  t->tabheight = (int *)NewAtom_N(int, MAXROW);

  for (i = 0; i < MAXROW; i++) {
    t->tabattr[i] = 0;
    t->tabheight[i] = 0;
  }
  for (j = 0; j < MAXCOL; j++) {
    t->tabwidth[j] = 0;
    t->minimum_width[j] = 0;
    t->fixed_width[j] = 0;
  }
  t->cell.maxcell = -1;
  t->cell.icell = -1;
  t->ntable = 0;
  t->tables_size = 0;
  t->tables = {};
  t->matrix = {};
  t->vector = {};
  t->linfo.prevchar = Strnew_size(8);
  set_prevchar(t->linfo.prevchar, "", 0);
  t->trattr = {};

  t->caption = Strnew();
  t->suspended_data = NULL;
  return t;
}

void table::check_row(int row) {
  int i, r;
  std::vector<std::vector<GeneralList<TextLine> *>> tabdata;
  table_attr **tabattr;
  int *tabheight;

  if (row < 0 || row >= MAXROW_LIMIT)
    return;
  if (row >= this->max_rowsize) {
    r = max(this->max_rowsize * 2, row + 1);
    if (r <= 0 || r > MAXROW_LIMIT)
      r = MAXROW_LIMIT;
    tabdata.resize(r);
    tabattr = (table_attr **)New_N(table_attr *, r);
    tabheight = (int *)NewAtom_N(int, r);
    for (i = 0; i < this->max_rowsize; i++) {
      tabdata[i] = this->tabdata[i];
      tabattr[i] = this->tabattr[i];
      tabheight[i] = this->tabheight[i];
    }
    for (; i < r; i++) {
      tabattr[i] = NULL;
      tabheight[i] = 0;
    }
    this->tabdata = tabdata;
    this->tabattr = tabattr;
    this->tabheight = tabheight;
    this->max_rowsize = r;
  }

  if (this->tabdata[row].empty()) {
    this->tabdata[row].resize(MAXCOL);
    this->tabattr[row] = (table_attr *)NewAtom_N(table_attr, MAXCOL);
    for (i = 0; i < MAXCOL; i++) {
      this->tabdata[row][i] = NULL;
      this->tabattr[row][i] = {};
    }
  }
}

void table::pushdata(int row, int col, const char *data) {
  this->check_row(row);
  if (this->tabdata[row][col] == NULL)
    this->tabdata[row][col] = GeneralList<TextLine>::newGeneralList();

  this->tabdata[row][col]->pushValue(TextLine::newTextLine(data));
}

void table::suspend_or_pushdata(const char *line) {
  if (this->flag & TBL_IN_COL)
    this->pushdata(this->row, this->col, line);
  else {
    if (!this->suspended_data)
      this->suspended_data = GeneralList<TextLine>::newGeneralList();
    this->suspended_data->pushValue(TextLine::newTextLine(line));
  }
}

#define PUSH_TAG(str, n) Strcat_char(tagbuf, *str), (void)n

int visible_length_offset = 0;
int visible_length(const char *str) {
  int len = 0, n, max_len = 0;
  auto status = R_ST_NORMAL;
  int prev_status = status;
  Str *tagbuf = Strnew();
  const char *t, *r2;
  // int amp_len = 0;

  while (*str) {
    prev_status = status;
    if (next_status(*str, &status)) {
      len++;
    }
    if (status == R_ST_TAG0) {
      Strclear(tagbuf);
      PUSH_TAG(str, n);
    } else if (status == R_ST_TAG || status == R_ST_DQUOTE ||
               status == R_ST_QUOTE || status == R_ST_EQL ||
               status == R_ST_VALUE) {
      PUSH_TAG(str, n);
    } else if (status == R_ST_AMP) {
      if (prev_status == R_ST_NORMAL) {
        Strclear(tagbuf);
        len--;
        // amp_len = 0;
      } else {
        PUSH_TAG(str, n);
        // amp_len++;
      }
    } else if (status == R_ST_NORMAL && prev_status == R_ST_AMP) {
      PUSH_TAG(str, n);
      r2 = tagbuf->ptr;
      auto _t = getescapecmd(&r2);
      t = _t.c_str();
      if (!*r2 && (*t == '\r' || *t == '\n')) {
        if (len > max_len)
          max_len = len;
        len = 0;
      } else
        len += get_strwidth(t) + get_strwidth(r2);
    } else if (status == R_ST_NORMAL && ST_IS_REAL_TAG(prev_status)) {
      ;
    } else if (*str == '\t') {
      len--;
      do {
        len++;
      } while ((visible_length_offset + len) % Tabstop != 0);
    } else if (*str == '\r' || *str == '\n') {
      len--;
      if (len > max_len)
        max_len = len;
      len = 0;
    }
    str++;
  }
  if (status == R_ST_AMP) {
    r2 = tagbuf->ptr;
    auto _t = getescapecmd(&r2);
    t = _t.c_str();
    if (*t != '\r' && *t != '\n')
      len += get_strwidth(t) + get_strwidth(r2);
  }
  return len > max_len ? len : max_len;
}

static int visible_length_plain(const char *str) {
  int len = 0, max_len = 0;

  while (*str) {
    if (*str == '\t') {
      do {
        len++;
      } while ((visible_length_offset + len) % Tabstop != 0);
      str++;
    } else if (*str == '\r' || *str == '\n') {
      if (len > max_len)
        max_len = len;
      len = 0;
      str++;
    } else {
      len++;
      str++;
    }
  }
  return len > max_len ? len : max_len;
}

static int maximum_visible_length(const char *str, int offset) {
  visible_length_offset = offset;
  return visible_length(str);
}

static int maximum_visible_length_plain(const char *str, int offset) {
  visible_length_offset = offset;
  return visible_length_plain(str);
}

void table::print_item(int row, int col, int width, Str *buf) {
  AlignMode alignment;

  TextLine *lbuf;
  if (this->tabdata[row][col]) {
    lbuf = this->tabdata[row][col]->popValue();
  } else {
    lbuf = NULL;
  }

  if (lbuf != NULL) {
    this->check_row(row);
    alignment = ALIGN_CENTER;
    if ((this->tabattr[row][col] & HTT_ALIGN) == HTT_LEFT)
      alignment = ALIGN_LEFT;
    else if ((this->tabattr[row][col] & HTT_ALIGN) == HTT_RIGHT)
      alignment = ALIGN_RIGHT;
    else if ((this->tabattr[row][col] & HTT_ALIGN) == HTT_CENTER)
      alignment = ALIGN_CENTER;
    if (DisableCenter && alignment == ALIGN_CENTER)
      alignment = ALIGN_LEFT;
    lbuf->align(width, alignment);
    Strcat(buf, lbuf->line);
  } else {
    lbuf = TextLine::newTextLine(NULL, 0);
    if (DisableCenter)
      lbuf->align(width, ALIGN_LEFT);
    else
      lbuf->align(width, ALIGN_CENTER);
    Strcat(buf, lbuf->line);
  }
}

#define T_TOP 0
#define T_MIDDLE 1
#define T_BOTTOM 2

void table::print_sep(int row, int type, int maxcol, Str *buf) {
  int forbid;
  int rule_mode;
  int i, k, l, m;

  if (row + 1 < 0 || row + 1 >= MAXROW_LIMIT)
    return;
  if (row >= 0)
    this->check_row(row);
  this->check_row(row + 1);
  if ((type == T_TOP || type == T_BOTTOM) &&
      this->border_mode == BORDER_THICK) {
    rule_mode = BORDER_THICK;
  } else {
    rule_mode = BORDER_THIN;
  }
  forbid = 1;
  if (type == T_TOP)
    forbid |= 2;
  else if (type == T_BOTTOM)
    forbid |= 8;
  else if (this->tabattr[row + 1][0] & HTT_Y) {
    forbid |= 4;
  }
  if (this->border_mode != BORDER_NOWIN) {
    push_symbol(buf, RULE(this->border_mode, forbid), symbol_width, 1);
  }
  for (i = 0; i <= maxcol; i++) {
    forbid = 10;
    if (type != T_BOTTOM && (this->tabattr[row + 1][i] & HTT_Y)) {
      if (this->tabattr[row + 1][i] & HTT_X) {
        goto do_last_sep;
      } else {
        for (k = row;
             k >= 0 && this->tabattr[k] && (this->tabattr[k][i] & HTT_Y); k--)
          ;
        m = this->tabwidth[i] + 2 * this->cellpadding;
        for (l = i + 1; l <= this->maxcol && (this->tabattr[row][l] & HTT_X);
             l++)
          m += this->tabwidth[l] + this->cellspacing;
        this->print_item(k, i, m, buf);
      }
    } else {
      int w = this->tabwidth[i] + 2 * this->cellpadding;
      if (RULE_WIDTH == 2)
        w = (w + 1) / RULE_WIDTH;
      push_symbol(buf, RULE(rule_mode, forbid), symbol_width, w);
    }
  do_last_sep:
    if (i < maxcol) {
      forbid = 0;
      if (type == T_TOP)
        forbid |= 2;
      else if (this->tabattr[row][i + 1] & HTT_X) {
        forbid |= 2;
      }
      if (type == T_BOTTOM)
        forbid |= 8;
      else {
        if (this->tabattr[row + 1][i + 1] & HTT_X) {
          forbid |= 8;
        }
        if (this->tabattr[row + 1][i + 1] & HTT_Y) {
          forbid |= 4;
        }
        if (this->tabattr[row + 1][i] & HTT_Y) {
          forbid |= 1;
        }
      }
      if (forbid != 15) /* forbid==15 means 'no rule at all' */
        push_symbol(buf, RULE(rule_mode, forbid), symbol_width, 1);
    }
  }
  forbid = 4;
  if (type == T_TOP)
    forbid |= 2;
  if (type == T_BOTTOM)
    forbid |= 8;
  if (this->tabattr[row + 1][maxcol] & HTT_Y) {
    forbid |= 1;
  }
  if (this->border_mode != BORDER_NOWIN)
    push_symbol(buf, RULE(this->border_mode, forbid), symbol_width, 1);
}

int table::get_spec_cell_width(int row, int col) {
  int i, w;

  w = this->tabwidth[col];
  for (i = col + 1; i <= this->maxcol; i++) {
    this->check_row(row);
    if (this->tabattr[row][i] & HTT_X)
      w += this->tabwidth[i] + this->cellspacing;
    else
      break;
  }
  return w;
}

void table::do_refill(HtmlParser *parser, int row, int col, int maxlimit) {
  if (this->tabdata[row].empty() || this->tabdata[row][col] == NULL) {
    return;
  }
  auto orgdata = this->tabdata[row][col];
  this->tabdata[row][col] = GeneralList<TextLine>::newGeneralList();

  html_feed_environ h_env(MAX_ENV_LEVEL, this->get_spec_cell_width(row, col), 0,
                          this->tabdata[row][col]);
  auto &obuf = h_env.obuf;
  obuf.flag |= RB_INTABLE;
  if (h_env.limit > maxlimit)
    h_env.limit = maxlimit;
  if (this->border_mode != BORDER_NONE && this->vcellpadding > 0)
    parser->do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
  for (auto l = orgdata->first; l != NULL; l = l->next) {
    if (TAG_IS(l->ptr->line->ptr, "<table_alt", 10)) {
      int id = -1;
      const char *p = l->ptr->line->ptr;
      std::shared_ptr<HtmlTag> tag;
      if ((tag = HtmlTag::parse(&p, true)) != NULL)
        tag->parsedtag_get_value(ATTR_TID, &id);
      if (id >= 0 && id < this->ntable && this->tables[id].ptr) {
        AlignMode alignment;
        GeneralList<TextLine>::ListItem *ti;
        struct table *t = this->tables[id].ptr;
        int limit = this->tables[id].indent + t->total_width;
        this->tables[id].ptr = NULL;
        parser->save_fonteffect(&h_env);
        obuf.flushline(h_env.buf, 0, 2, h_env.limit);
        if (t->vspace > 0 && !(obuf.flag & RB_IGNORE_P))
          parser->do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
        if (h_env.obuf.RB_GET_ALIGN() == RB_CENTER) {
          alignment = ALIGN_CENTER;
        } else if (h_env.obuf.RB_GET_ALIGN() == RB_RIGHT) {
          alignment = ALIGN_RIGHT;
        } else {
          alignment = ALIGN_LEFT;
        }

        if (alignment != ALIGN_LEFT) {
          for (ti = this->tables[id].buf->first; ti != NULL; ti = ti->next) {
            ti->ptr->align(h_env.limit, alignment);
          }
        }
        h_env.buf->appendGeneralList(this->tables[id].buf);
        if (h_env.obuf.maxlimit < limit)
          h_env.obuf.maxlimit = limit;
        parser->restore_fonteffect(&h_env);
        obuf.flag &= ~RB_IGNORE_P;
        h_env.obuf.blank_lines = 0;
        if (t->vspace > 0) {
          parser->do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
          obuf.flag |= RB_IGNORE_P;
        }
      }
    } else
      parser->HTMLlineproc1(l->ptr->line->ptr, &h_env);
  }
  if (obuf.status != R_ST_NORMAL) {
    obuf.status = R_ST_EOL;
    parser->HTMLlineproc1("\n", &h_env);
  }
  parser->completeHTMLstream(&h_env);
  obuf.flushline(h_env.buf, 0, 2, h_env.limit);
  if (this->border_mode == BORDER_NONE) {
    int rowspan = this->table_rowspan(row, col);
    if (row + rowspan <= this->maxrow) {
      if (this->vcellpadding > 0 && !(obuf.flag & RB_IGNORE_P))
        parser->do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
    } else {
      if (this->vspace > 0) {
        h_env.purgeline();
      }
    }
  } else {
    if (this->vcellpadding > 0) {
      if (!(obuf.flag & RB_IGNORE_P))
        parser->do_blankline(&h_env, &obuf, 0, 0, h_env.limit);
    } else {
      h_env.purgeline();
    }
  }
  int colspan, icell;
  if ((colspan = this->table_colspan(row, col)) > 1) {
    struct table_cell *cell = &this->cell;
    int k;
    k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
                       cell->index, cell->maxcell + 1);
    icell = cell->index[k];
    if (cell->minimum_width[icell] < h_env.obuf.maxlimit)
      cell->minimum_width[icell] = h_env.obuf.maxlimit;
  } else {
    if (this->minimum_width[col] < h_env.obuf.maxlimit)
      this->minimum_width[col] = h_env.obuf.maxlimit;
  }
}

int table::table_rule_width() const {
  if (this->border_mode == BORDER_NONE)
    return 1;
  return RULE_WIDTH;
}

static void check_cell_height(int *tabheight, int *cellheight, short *row,
                              short *rowspan, short maxcell, short *indexarray,
                              int space, int dir) {
  int i, j, k, brow, erow;
  int sheight, height;

  for (k = 0; k <= maxcell; k++) {
    j = indexarray[k];
    if (cellheight[j] <= 0)
      continue;
    brow = row[j];
    erow = brow + rowspan[j];
    sheight = 0;
    for (i = brow; i < erow; i++)
      sheight += tabheight[i];

    height = cellheight[j] - (rowspan[j] - 1) * space;
    if (height > sheight) {
      int w = (height - sheight) / rowspan[j];
      int r = (height - sheight) % rowspan[j];
      for (i = brow; i < erow; i++)
        tabheight[i] += w;
      /* dir {0: horizontal, 1: vertical} */
      if (dir == 1 && r > 0)
        r = rowspan[j];
      for (i = 1; i <= r; i++)
        tabheight[erow - i]++;
    }
  }
}

static void check_cell_width(short *tabwidth, const short *cellwidth,
                             const short *col, const short *colspan,
                             short maxcell, const short *indexarray, int space,
                             int dir) {
  int i, j, k, bcol, ecol;
  int swidth, width;

  for (k = 0; k <= maxcell; k++) {
    j = indexarray[k];
    if (cellwidth[j] <= 0)
      continue;
    bcol = col[j];
    ecol = bcol + colspan[j];
    swidth = 0;
    for (i = bcol; i < ecol; i++)
      swidth += tabwidth[i];

    width = cellwidth[j] - (colspan[j] - 1) * space;
    if (width > swidth) {
      int w = (width - swidth) / colspan[j];
      int r = (width - swidth) % colspan[j];
      for (i = bcol; i < ecol; i++)
        tabwidth[i] += w;
      /* dir {0: horizontal, 1: vertical} */
      if (dir == 1 && r > 0)
        r = colspan[j];
      for (i = 1; i <= r; i++)
        tabwidth[ecol - i]++;
    }
  }
}

void table::check_minimum_width(short *tabwidth) const {
  for (int i = 0; i <= this->maxcol; i++) {
    if (tabwidth[i] < this->minimum_width[i])
      tabwidth[i] = this->minimum_width[i];
  }

  check_cell_width(tabwidth, cell.minimum_width, cell.col, cell.colspan,
                   cell.maxcell, cell.index, this->cellspacing, 0);
}

void table::check_maximum_width() {
  struct table_cell *cell = &this->cell;
  int i, j, bcol, ecol;
  int swidth, width;

  cell->necell = 0;
  for (j = 0; j <= cell->maxcell; j++) {
    bcol = cell->col[j];
    ecol = bcol + cell->colspan[j];
    swidth = 0;
    for (i = bcol; i < ecol; i++)
      swidth += this->tabwidth[i];

    width = cell->width[j] - (cell->colspan[j] - 1) * this->cellspacing;
    if (width > swidth) {
      cell->eindex[cell->necell] = j;
      cell->necell++;
    }
  }
}

void table::set_integered_width(double *dwidth, short *iwidth) {
  int i, j, n, bcol, ecol, step;
  struct table_cell *cell = &this->cell;
  int rulewidth = this->table_rule_width();

  auto indexarray = (short *)NewAtom_N(short, this->maxcol + 1);
  auto mod = (double *)NewAtom_N(double, this->maxcol + 1);
  for (i = 0; i <= this->maxcol; i++) {
    iwidth[i] = static_cast<short>(
        ceil_at_intervals(static_cast<int>(ceil(dwidth[i])), rulewidth));
    mod[i] = (double)iwidth[i] - dwidth[i];
  }

  auto sum = 0.;
  double x = 0;
  for (int k = 0; k <= this->maxcol; k++) {
    x = mod[k];
    sum += x;
    i = bsearch_double(x, mod, indexarray, k);
    if (k > i) {
      int ii;
      for (ii = k; ii > i; ii--)
        indexarray[ii] = indexarray[ii - 1];
    }
    indexarray[i] = k;
  }

  auto fixed = (char *)NewAtom_N(char, this->maxcol + 1);
  memset(fixed, 0, this->maxcol + 1);
  for (step = 0; step < 2; step++) {
    for (i = 0; i <= this->maxcol; i += n) {
      int nn;
      short *idx;
      double nsum;
      if (sum < 0.5)
        return;
      for (n = 0; i + n <= this->maxcol; n++) {
        int ii = indexarray[i + n];
        if (n == 0)
          x = mod[ii];
        else if (fabs(mod[ii] - x) > 1e-6)
          break;
      }
      for (int k = 0; k < n; k++) {
        int ii = indexarray[i + k];
        if (fixed[ii] < 2 && iwidth[ii] - rulewidth < this->minimum_width[ii])
          fixed[ii] = 2;
        if (fixed[ii] < 1 && iwidth[ii] - rulewidth < this->tabwidth[ii] &&
            (double)rulewidth - mod[ii] > 0.5)
          fixed[ii] = 1;
      }
      idx = (short *)NewAtom_N(short, n);
      for (int k = 0; k < cell->maxcell; k++) {
        int kk, w, width, m;
        j = cell->index[k];
        bcol = cell->col[j];
        ecol = bcol + cell->colspan[j];
        m = 0;
        for (kk = 0; kk < n; kk++) {
          int ii = indexarray[i + kk];
          if (ii >= bcol && ii < ecol) {
            idx[m] = ii;
            m++;
          }
        }
        if (m == 0)
          continue;
        width = (cell->colspan[j] - 1) * this->cellspacing;
        for (kk = bcol; kk < ecol; kk++)
          width += iwidth[kk];
        w = 0;
        for (kk = 0; kk < m; kk++) {
          if (fixed[(int)idx[kk]] < 2)
            w += rulewidth;
        }
        if (width - w < cell->minimum_width[j]) {
          for (kk = 0; kk < m; kk++) {
            if (fixed[(int)idx[kk]] < 2)
              fixed[(int)idx[kk]] = 2;
          }
        }
        w = 0;
        for (kk = 0; kk < m; kk++) {
          if (fixed[(int)idx[kk]] < 1 &&
              (double)rulewidth - mod[(int)idx[kk]] > 0.5)
            w += rulewidth;
        }
        if (width - w < cell->width[j]) {
          for (kk = 0; kk < m; kk++) {
            if (fixed[(int)idx[kk]] < 1 &&
                (double)rulewidth - mod[(int)idx[kk]] > 0.5)
              fixed[(int)idx[kk]] = 1;
          }
        }
      }
      nn = 0;
      for (int k = 0; k < n; k++) {
        int ii = indexarray[i + k];
        if (fixed[ii] <= step)
          nn++;
      }
      nsum = sum - (double)(nn * rulewidth);
      if (nsum < 0. && fabs(sum) <= fabs(nsum))
        return;
      for (int k = 0; k < n; k++) {
        int ii = indexarray[i + k];
        if (fixed[ii] <= step) {
          iwidth[ii] -= rulewidth;
          fixed[ii] = 3;
        }
      }
      sum = nsum;
    }
  }
}

static double correlation_coefficient(double sxx, double syy, double sxy) {
  double coe, tmp;
  tmp = sxx * syy;
  if (tmp < Tiny)
    tmp = Tiny;
  coe = sxy / sqrt(tmp);
  if (coe > 1.)
    return 1.;
  if (coe < -1.)
    return -1.;
  return coe;
}

static double correlation_coefficient2(double sxx, double syy, double sxy) {
  double coe, tmp;
  tmp = (syy + sxx - 2 * sxy) * sxx;
  if (tmp < Tiny)
    tmp = Tiny;
  coe = (sxx - sxy) / sqrt(tmp);
  if (coe > 1.)
    return 1.;
  if (coe < -1.)
    return -1.;
  return coe;
}

static double recalc_width(double old, double swidth, int cwidth, double sxx,
                           double syy, double sxy, int is_inclusive) {
  double delta = swidth - (double)cwidth;
  double rat = sxy / sxx, coe = correlation_coefficient(sxx, syy, sxy), w, ww;
  if (old < 0.)
    old = 0.;
  if (fabs(coe) < 1e-5)
    return old;
  w = rat * old;
  ww = delta;
  if (w > 0.) {
    double wmin = 5e-3 * sqrt(syy * (1. - coe * coe));
    if (swidth < 0.2 && cwidth > 0 && is_inclusive) {
      double coe1 = correlation_coefficient2(sxx, syy, sxy);
      if (coe > 0.9 || coe1 > 0.9)
        return 0.;
    }
    if (wmin > 0.05)
      wmin = 0.05;
    if (ww < 0.)
      ww = 0.;
    ww += wmin;
  } else {
    double wmin = 5e-3 * sqrt(syy) * fabs(coe);
    if (rat > -0.001)
      return old;
    if (wmin > 0.01)
      wmin = 0.01;
    if (ww > 0.)
      ww = 0.;
    ww -= wmin;
  }
  if (w > ww)
    return ww / rat;
  return old;
}

int table::check_compressible_cell(Matrix *minv, double *newwidth,
                                   double *swidth, short *cwidth,
                                   double totalwidth, double *Sxx, int icol,
                                   int icell, double sxx, int corr) {
  struct table_cell *cell = &this->cell;
  int i, j, k, m, bcol, ecol, span;
  double delta, owidth;
  double dmax, dmin, sxy;
  int rulewidth = this->table_rule_width();

  if (sxx < 10.)
    return corr;

  if (icol >= 0) {
    owidth = newwidth[icol];
    delta = newwidth[icol] - (double)this->tabwidth[icol];
    bcol = icol;
    ecol = bcol + 1;
  } else if (icell >= 0) {
    owidth = swidth[icell];
    delta = swidth[icell] - (double)cwidth[icell];
    bcol = cell->col[icell];
    ecol = bcol + cell->colspan[icell];
  } else {
    owidth = totalwidth;
    delta = totalwidth;
    bcol = 0;
    ecol = this->maxcol + 1;
  }

  dmin = delta;
  dmax = -1.;
  for (k = 0; k <= cell->maxcell; k++) {
    int bcol1, ecol1;
    int is_inclusive = 0;
    if (dmin <= 0.)
      goto _end;
    j = cell->index[k];
    if (j == icell)
      continue;
    bcol1 = cell->col[j];
    ecol1 = bcol1 + cell->colspan[j];
    sxy = 0.;
    for (m = bcol1; m < ecol1; m++) {
      for (i = bcol; i < ecol; i++)
        sxy += minv->m_entry(i, m);
    }
    if (bcol1 >= bcol && ecol1 <= ecol) {
      is_inclusive = 1;
    }
    if (sxy > 0.)
      dmin = recalc_width(dmin, swidth[j], cwidth[j], sxx, Sxx[j], sxy,
                          is_inclusive);
    else
      dmax = recalc_width(dmax, swidth[j], cwidth[j], sxx, Sxx[j], sxy,
                          is_inclusive);
  }
  for (m = 0; m <= this->maxcol; m++) {
    int is_inclusive = 0;
    if (dmin <= 0.)
      goto _end;
    if (m == icol)
      continue;
    sxy = 0.;
    for (i = bcol; i < ecol; i++)
      sxy += minv->m_entry(i, m);
    if (m >= bcol && m < ecol) {
      is_inclusive = 1;
    }
    if (sxy > 0.)
      dmin = recalc_width(dmin, newwidth[m], this->tabwidth[m], sxx,
                          minv->m_entry(m, m), sxy, is_inclusive);
    else
      dmax = recalc_width(dmax, newwidth[m], this->tabwidth[m], sxx,
                          minv->m_entry(m, m), sxy, is_inclusive);
  }
_end:
  if (dmax > 0. && dmin > dmax)
    dmin = dmax;
  span = ecol - bcol;
  if ((span == this->maxcol + 1 && dmin >= 0.) ||
      (span != this->maxcol + 1 && dmin > rulewidth * 0.5)) {
    int nwidth = ceil_at_intervals(round(owidth - dmin), rulewidth);
    this->correct_table_matrix(bcol, ecol - bcol, nwidth, 1.);
    corr++;
  }
  return corr;
}

#define MAX_ITERATION 10
int table::check_table_width(double *newwidth, Matrix *minv, int itr) {
  int i, j, k, m, bcol, ecol;
  int corr = 0;
  struct table_cell *cell = &this->cell;
#ifdef __GNUC__
  short orgwidth[this->maxcol >= 0 ? this->maxcol + 1 : 1];
  short corwidth[this->maxcol >= 0 ? this->maxcol + 1 : 1];
  short cwidth[cell->maxcell >= 0 ? cell->maxcell + 1 : 1];
  double swidth[cell->maxcell >= 0 ? cell->maxcell + 1 : 1];
#else  /* __GNUC__ */
  short orgwidth[MAXCOL], corwidth[MAXCOL];
  short cwidth[MAXCELL];
  double swidth[MAXCELL];
#endif /* __GNUC__ */
  double twidth, sxy, *Sxx, stotal;

  twidth = 0.;
  stotal = 0.;
  for (i = 0; i <= this->maxcol; i++) {
    twidth += newwidth[i];
    stotal += minv->m_entry(i, i);
    for (m = 0; m < i; m++) {
      stotal += 2 * minv->m_entry(i, m);
    }
  }

  Sxx = (double *)NewAtom_N(double, cell->maxcell + 1);
  for (k = 0; k <= cell->maxcell; k++) {
    j = cell->index[k];
    bcol = cell->col[j];
    ecol = bcol + cell->colspan[j];
    swidth[j] = 0.;
    for (i = bcol; i < ecol; i++)
      swidth[j] += newwidth[i];
    cwidth[j] = cell->width[j] - (cell->colspan[j] - 1) * this->cellspacing;
    Sxx[j] = 0.;
    for (i = bcol; i < ecol; i++) {
      Sxx[j] += minv->m_entry(i, i);
      for (m = bcol; m <= ecol; m++) {
        if (m < i)
          Sxx[j] += 2 * minv->m_entry(i, m);
      }
    }
  }

  /* compress table */
  corr = this->check_compressible_cell(minv, newwidth, swidth, cwidth, twidth,
                                       Sxx, -1, -1, stotal, corr);
  if (itr < MAX_ITERATION && corr > 0)
    return corr;

  /* compress multicolumn cell */
  for (k = cell->maxcell; k >= 0; k--) {
    j = cell->index[k];
    corr = this->check_compressible_cell(minv, newwidth, swidth, cwidth, twidth,
                                         Sxx, -1, j, Sxx[j], corr);
    if (itr < MAX_ITERATION && corr > 0)
      return corr;
  }

  /* compress single column cell */
  for (i = 0; i <= this->maxcol; i++) {
    corr = this->check_compressible_cell(minv, newwidth, swidth, cwidth, twidth,
                                         Sxx, i, -1, minv->m_entry(i, i), corr);
    if (itr < MAX_ITERATION && corr > 0)
      return corr;
  }

  for (i = 0; i <= this->maxcol; i++)
    corwidth[i] = orgwidth[i] = round(newwidth[i]);

  this->check_minimum_width(corwidth);

  for (i = 0; i <= this->maxcol; i++) {
    double sx = sqrt(minv->m_entry(i, i));
    if (sx < 0.1)
      continue;
    if (orgwidth[i] < this->minimum_width[i] &&
        corwidth[i] == this->minimum_width[i]) {
      double w = (sx > 0.5) ? 0.5 : sx * 0.2;
      sxy = 0.;
      for (m = 0; m <= this->maxcol; m++) {
        if (m == i)
          continue;
        sxy += minv->m_entry(i, m);
      }
      if (sxy <= 0.) {
        this->correct_table_matrix(i, 1, this->minimum_width[i], w);
        corr++;
      }
    }
  }

  for (k = 0; k <= cell->maxcell; k++) {
    int nwidth = 0, mwidth;
    double sx;

    j = cell->index[k];
    sx = sqrt(Sxx[j]);
    if (sx < 0.1)
      continue;
    bcol = cell->col[j];
    ecol = bcol + cell->colspan[j];
    for (i = bcol; i < ecol; i++)
      nwidth += corwidth[i];
    mwidth =
        cell->minimum_width[j] - (cell->colspan[j] - 1) * this->cellspacing;
    if (mwidth > swidth[j] && mwidth == nwidth) {
      double w = (sx > 0.5) ? 0.5 : sx * 0.2;

      sxy = 0.;
      for (i = bcol; i < ecol; i++) {
        for (m = 0; m <= this->maxcol; m++) {
          if (m >= bcol && m < ecol)
            continue;
          sxy += minv->m_entry(i, m);
        }
      }
      if (sxy <= 0.) {
        this->correct_table_matrix(bcol, cell->colspan[j], mwidth, w);
        corr++;
      }
    }
  }

  if (itr >= MAX_ITERATION)
    return 0;
  else
    return corr;
}

void table::check_table_height() {
  int i, j, k;
  struct {
    short *row;
    short *rowspan;
    short *indexarray;
    short maxcell;
    short size;
    int *height;
  } cell = {0};
  int space = 0;

  cell.size = 0;
  cell.maxcell = -1;

  for (j = 0; j <= this->maxrow; j++) {
    if (!this->tabattr[j])
      continue;
    for (i = 0; i <= this->maxcol; i++) {
      int t_dep, rowspan;
      if (this->tabattr[j][i] & (HTT_X | HTT_Y))
        continue;

      if (this->tabdata[j][i] == NULL)
        t_dep = 0;
      else
        t_dep = this->tabdata[j][i]->nitem;

      rowspan = this->table_rowspan(j, i);
      if (rowspan > 1) {
        int c = cell.maxcell + 1;
        k = bsearch_2short(rowspan, cell.rowspan, j, cell.row, this->maxrow + 1,
                           cell.indexarray, c);
        if (k <= cell.maxcell) {
          int idx = cell.indexarray[k];
          if (cell.row[idx] == j && cell.rowspan[idx] == rowspan)
            c = idx;
        }
        if (c >= MAXROWCELL)
          continue;
        if (c >= cell.size) {
          if (cell.size == 0) {
            cell.size = max(MAXCELL, c + 1);
            cell.row = (short *)NewAtom_N(short, cell.size);
            cell.rowspan = (short *)NewAtom_N(short, cell.size);
            cell.indexarray = (short *)NewAtom_N(short, cell.size);
            cell.height = (int *)NewAtom_N(int, cell.size);
          } else {
            cell.size = max(cell.size + MAXCELL, c + 1);
            cell.row = (short *)New_Reuse(short, cell.row, cell.size);
            cell.rowspan = (short *)New_Reuse(short, cell.rowspan, cell.size);
            cell.indexarray =
                (short *)New_Reuse(short, cell.indexarray, cell.size);
            cell.height = (int *)New_Reuse(int, cell.height, cell.size);
          }
        }
        if (c > cell.maxcell) {
          cell.maxcell++;
          cell.row[cell.maxcell] = j;
          cell.rowspan[cell.maxcell] = rowspan;
          cell.height[cell.maxcell] = 0;
          if (cell.maxcell > k) {
            int ii;
            for (ii = cell.maxcell; ii > k; ii--)
              cell.indexarray[ii] = cell.indexarray[ii - 1];
          }
          cell.indexarray[k] = cell.maxcell;
        }

        if (cell.height[c] < t_dep)
          cell.height[c] = t_dep;
        continue;
      }
      if (this->tabheight[j] < t_dep)
        this->tabheight[j] = t_dep;
    }
  }

  switch (this->border_mode) {
  case BORDER_THIN:
  case BORDER_THICK:
  case BORDER_NOWIN:
    space = 1;
    break;
  case BORDER_NONE:
    space = 0;
  }
  check_cell_height(this->tabheight, cell.height, cell.row, cell.rowspan,
                    cell.maxcell, cell.indexarray, space, 1);
}

int table::get_table_width(const short *orgwidth, const short *cellwidth,
                           TableWidthFlags flag) const {
#ifdef __GNUC__
  short newwidth[this->maxcol >= 0 ? this->maxcol + 1 : 1];
#else  /* not __GNUC__ */
  short newwidth[MAXCOL];
#endif /* not __GNUC__ */
  int i;
  int swidth;
  // struct table_cell *cell = &this->cell;
  int rulewidth = this->table_rule_width();

  for (i = 0; i <= this->maxcol; i++)
    newwidth[i] = max(orgwidth[i], 0);

  if (flag & CHECK_FIXED) {
#ifdef __GNUC__
    short ccellwidth[cell.maxcell >= 0 ? cell.maxcell + 1 : 1];
#else  /* not __GNUC__ */
    short ccellwidth[MAXCELL];
#endif /* not __GNUC__ */
    for (i = 0; i <= this->maxcol; i++) {
      if (newwidth[i] < this->fixed_width[i])
        newwidth[i] = this->fixed_width[i];
    }
    for (i = 0; i <= cell.maxcell; i++) {
      ccellwidth[i] = cellwidth[i];
      if (ccellwidth[i] < cell.fixed_width[i])
        ccellwidth[i] = cell.fixed_width[i];
    }
    check_cell_width(newwidth, ccellwidth, cell.col, cell.colspan, cell.maxcell,
                     cell.index, this->cellspacing, 0);
  } else {
    check_cell_width(newwidth, cellwidth, cell.col, cell.colspan, cell.maxcell,
                     cell.index, this->cellspacing, 0);
  }
  if (flag & CHECK_MINIMUM)
    this->check_minimum_width(newwidth);

  swidth = 0;
  for (i = 0; i <= this->maxcol; i++) {
    swidth += ceil_at_intervals(newwidth[i], rulewidth);
  }
  swidth += this->table_border_width();
  return swidth;
}

#define MAX_COTABLE_LEVEL 100
static int cotable_level;

void initRenderTable(void) { cotable_level = 0; }

void table::renderCoTable(HtmlParser *parser, int maxlimit) {
  struct table *t;
  int i, col, row;
  int indent, maxwidth;

  if (cotable_level >= MAX_COTABLE_LEVEL)
    return; /* workaround to prevent infinite recursion */
  cotable_level++;

  for (i = 0; i < this->ntable; i++) {
    t = this->tables[i].ptr;
    if (t == NULL)
      continue;
    col = this->tables[i].col;
    row = this->tables[i].row;
    indent = this->tables[i].indent;

    html_feed_environ h_env(MAX_ENV_LEVEL, this->get_spec_cell_width(row, col),
                            indent, this->tables[i].buf);
    this->check_row(row);
    if (h_env.limit > maxlimit)
      h_env.limit = maxlimit;
    if (t->total_width == 0)
      maxwidth = h_env.limit - indent;
    else if (t->total_width > 0)
      maxwidth = t->total_width;
    else
      maxwidth = t->total_width = -t->total_width * h_env.limit / 100;
    t->renderTable(parser, maxwidth, &h_env);
  }
}

void table::make_caption(HtmlParser *parser, struct html_feed_environ *h_env) {
  int limit;

  if (this->caption->length <= 0)
    return;

  if (this->total_width > 0)
    limit = this->total_width;
  else
    limit = h_env->limit;

  html_feed_environ henv(MAX_ENV_LEVEL, limit, h_env->envs[h_env->envc].indent,
                         GeneralList<TextLine>::newGeneralList());
  parser->HTMLlineproc1("<center>", &henv);
  parser->parseLine(this->caption->ptr, &henv, false);
  parser->HTMLlineproc1("</center>", &henv);

  if (this->total_width < henv.obuf.maxlimit)
    this->total_width = henv.obuf.maxlimit;
  limit = h_env->limit;
  h_env->limit = this->total_width;
  parser->HTMLlineproc1("<center>", h_env);
  parser->parseLine(this->caption->ptr, h_env, false);
  parser->HTMLlineproc1("</center>", h_env);
  h_env->limit = limit;
}

void table::renderTable(HtmlParser *parser, int max_width,
                        struct html_feed_environ *h_env) {
  int i, j, w, r, h;
  Str *renderbuf;
  short new_tabwidth[MAXCOL] = {0};
  int itr;
  int width;
  Str *vrulea = NULL, *vruleb = NULL, *vrulec = NULL;

  this->total_height = 0;
  if (this->maxcol < 0) {
    this->make_caption(parser, h_env);
    return;
  }

  if (this->sloppy_width > max_width)
    max_width = this->sloppy_width;

  int rulewidth = this->table_rule_width();

  max_width -= this->table_border_width();

  if (rulewidth > 1)
    max_width = floor_at_intervals(max_width, rulewidth);

  if (max_width < rulewidth)
    max_width = rulewidth;

#define MAX_TABWIDTH 10000
  if (max_width > MAX_TABWIDTH)
    max_width = MAX_TABWIDTH;

  this->check_maximum_width();

  if (this->maxcol == 0) {
    if (this->tabwidth[0] > max_width)
      this->tabwidth[0] = max_width;
    if (this->total_width > 0)
      this->tabwidth[0] = max_width;
    else if (this->fixed_width[0] > 0)
      this->tabwidth[0] = this->fixed_width[0];
    if (this->tabwidth[0] < this->minimum_width[0])
      this->tabwidth[0] = this->minimum_width[0];
  } else {
    this->set_table_matrix(max_width);

    itr = 0;
    Matrix mat(this->maxcol + 1);
    std::vector<int> pivot;
    pivot.resize(this->maxcol + 1);
    Vector newwidth(this->maxcol + 1);
    Matrix minv(this->maxcol + 1);
    do {
      this->matrix.copy_to(&mat);
      mat.LUfactor(pivot.data());
      mat.LUsolve(pivot.data(), &this->vector, &newwidth);
      minv = mat.LUinverse(pivot.data());
      itr++;
    } while (this->check_table_width(newwidth.ve.data(), &minv, itr));
    this->set_integered_width(newwidth.ve.data(), new_tabwidth);
    this->check_minimum_width(new_tabwidth);
    for (int i = 0; i <= this->maxcol; i++) {
      this->tabwidth[i] = new_tabwidth[i];
    }
  }

  this->check_minimum_width(this->tabwidth);
  for (i = 0; i <= this->maxcol; i++)
    this->tabwidth[i] = ceil_at_intervals(this->tabwidth[i], rulewidth);

  this->renderCoTable(parser, h_env->limit);

  for (i = 0; i <= this->maxcol; i++) {
    for (j = 0; j <= this->maxrow; j++) {
      this->check_row(j);
      if (this->tabattr[j][i] & HTT_Y)
        continue;
      this->do_refill(parser, j, i, h_env->limit);
    }
  }

  this->check_minimum_width(this->tabwidth);
  this->total_width = 0;
  for (i = 0; i <= this->maxcol; i++) {
    this->tabwidth[i] = ceil_at_intervals(this->tabwidth[i], rulewidth);
    this->total_width += this->tabwidth[i];
  }

  this->total_width += this->table_border_width();

  this->check_table_height();

  for (i = 0; i <= this->maxcol; i++) {
    for (j = 0; j <= this->maxrow; j++) {
      int k;
      if ((this->tabattr[j][i] & HTT_Y) || (this->tabattr[j][i] & HTT_TOP) ||
          (this->tabdata[j][i] == NULL))
        continue;
      h = this->tabheight[j];
      for (k = j + 1; k <= this->maxrow; k++) {
        if (!(this->tabattr[k][i] & HTT_Y))
          break;
        h += this->tabheight[k];
        switch (this->border_mode) {
        case BORDER_THIN:
        case BORDER_THICK:
        case BORDER_NOWIN:
          h += 1;
          break;
        }
      }
      h -= this->tabdata[j][i]->nitem;
      if (this->tabattr[j][i] & HTT_MIDDLE)
        h /= 2;
      if (h <= 0)
        continue;

      GeneralList<TextLine> *l;
      l = GeneralList<TextLine>::newGeneralList();
      for (k = 0; k < h; k++) {
        l->pushValue(TextLine::newTextLine(NULL, 0));
      }
      l->appendGeneralList(this->tabdata[j][i]);
      this->tabdata[j][i] = l;
    }
  }

  /* table output */
  width = this->total_width;

  this->make_caption(parser, h_env);

  parser->HTMLlineproc1("<pre for_table>", h_env);
  switch (this->border_mode) {
  case BORDER_THIN:
  case BORDER_THICK:
    renderbuf = Strnew();
    this->print_sep(-1, T_TOP, this->maxcol, renderbuf);
    parser->push_render_image(renderbuf, width, this->total_width, h_env);
    this->total_height += 1;
    break;
  }
  vruleb = Strnew();
  switch (this->border_mode) {
  case BORDER_THIN:
  case BORDER_THICK:
    vrulea = Strnew();
    vrulec = Strnew();
    push_symbol(vrulea, TK_VERTICALBAR(this->border_mode), symbol_width, 1);
    for (i = 0; i < this->cellpadding; i++) {
      Strcat_char(vrulea, ' ');
      Strcat_char(vruleb, ' ');
      Strcat_char(vrulec, ' ');
    }
    push_symbol(vrulec, TK_VERTICALBAR(this->border_mode), symbol_width, 1);
  case BORDER_NOWIN:
    push_symbol(vruleb, TK_VERTICALBAR(BORDER_THIN), symbol_width, 1);
    for (i = 0; i < this->cellpadding; i++)
      Strcat_char(vruleb, ' ');
    break;
  case BORDER_NONE:
    for (i = 0; i < this->cellspacing; i++)
      Strcat_char(vruleb, ' ');
  }

  for (r = 0; r <= this->maxrow; r++) {
    for (h = 0; h < this->tabheight[r]; h++) {
      renderbuf = Strnew();
      if (this->border_mode == BORDER_THIN || this->border_mode == BORDER_THICK)
        Strcat(renderbuf, vrulea);
      for (i = 0; i <= this->maxcol; i++) {
        this->check_row(r);
        if (!(this->tabattr[r][i] & HTT_X)) {
          w = this->tabwidth[i];
          for (j = i + 1; j <= this->maxcol && (this->tabattr[r][j] & HTT_X);
               j++)
            w += this->tabwidth[j] + this->cellspacing;
          if (this->tabattr[r][i] & HTT_Y) {
            for (j = r - 1;
                 j >= 0 && this->tabattr[j] && (this->tabattr[j][i] & HTT_Y);
                 j--)
              ;
            this->print_item(j, i, w, renderbuf);
          } else
            this->print_item(r, i, w, renderbuf);
        }
        if (i < this->maxcol && !(this->tabattr[r][i + 1] & HTT_X))
          Strcat(renderbuf, vruleb);
      }
      switch (this->border_mode) {
      case BORDER_THIN:
      case BORDER_THICK:
        Strcat(renderbuf, vrulec);
        this->total_height += 1;
        break;
      }
      parser->push_render_image(renderbuf, width, this->total_width, h_env);
    }
    if (r < this->maxrow && this->border_mode != BORDER_NONE) {
      renderbuf = Strnew();
      this->print_sep(r, T_MIDDLE, this->maxcol, renderbuf);
      parser->push_render_image(renderbuf, width, this->total_width, h_env);
    }
    this->total_height += this->tabheight[r];
  }
  switch (this->border_mode) {
  case BORDER_THIN:
  case BORDER_THICK:
    renderbuf = Strnew();
    this->print_sep(this->maxrow, T_BOTTOM, this->maxcol, renderbuf);
    parser->push_render_image(renderbuf, width, this->total_width, h_env);
    this->total_height += 1;
    break;
  }
  if (this->total_height == 0) {
    renderbuf = Strnew_charp(" ");
    this->total_height++;
    this->total_width = 1;
    parser->push_render_image(renderbuf, 1, this->total_width, h_env);
  }
  parser->HTMLlineproc1("</pre>", h_env);
}

#ifdef TABLE_NO_COMPACT
#define THR_PADDING 2
#else
#define THR_PADDING 4
#endif

struct table *table::begin_table(int border, int spacing, int padding,
                                 int vspace) {
  int mincell = minimum_cellspacing(border);
  int rcellspacing;
  int mincell_pixels = round(mincell * pixel_per_char);
  int ppc = round(pixel_per_char);

  auto t = table::newTable();
  t->row = t->col = -1;
  t->maxcol = -1;
  t->maxrow = -1;
  t->border_mode = border;
  t->flag = 0;
  if (border == BORDER_NOWIN)
    t->flag |= TBL_EXPAND_OK;

  rcellspacing = spacing + 2 * padding;
  switch (border) {
  case BORDER_THIN:
  case BORDER_THICK:
  case BORDER_NOWIN:
    t->cellpadding = padding - (mincell_pixels - 4) / 2;
    break;
  case BORDER_NONE:
    t->cellpadding = rcellspacing - mincell_pixels;
  }
  if (t->cellpadding >= ppc)
    t->cellpadding /= ppc;
  else if (t->cellpadding > 0)
    t->cellpadding = 1;
  else
    t->cellpadding = 0;

  switch (border) {
  case BORDER_THIN:
  case BORDER_THICK:
  case BORDER_NOWIN:
    t->cellspacing = 2 * t->cellpadding + mincell;
    break;
  case BORDER_NONE:
    t->cellspacing = t->cellpadding + mincell;
  }

  if (border == BORDER_NONE) {
    if (rcellspacing / 2 + vspace <= 1)
      t->vspace = 0;
    else
      t->vspace = 1;
  } else {
    if (vspace < ppc)
      t->vspace = 0;
    else
      t->vspace = 1;
  }

  if (border == BORDER_NONE) {
    if (rcellspacing <= THR_PADDING)
      t->vcellpadding = 0;
    else
      t->vcellpadding = 1;
  } else {
    if (padding < 2 * ppc - 2)
      t->vcellpadding = 0;
    else
      t->vcellpadding = 1;
  }

  return t;
}

void table::end_table() {
  struct table_cell *cell = &this->cell;
  int i;
  int rulewidth = this->table_rule_width();
  if (rulewidth > 1) {
    if (this->total_width > 0)
      this->total_width = ceil_at_intervals(this->total_width, rulewidth);
    for (i = 0; i <= this->maxcol; i++) {
      this->minimum_width[i] =
          ceil_at_intervals(this->minimum_width[i], rulewidth);
      this->tabwidth[i] = ceil_at_intervals(this->tabwidth[i], rulewidth);
      if (this->fixed_width[i] > 0)
        this->fixed_width[i] =
            ceil_at_intervals(this->fixed_width[i], rulewidth);
    }
    for (i = 0; i <= cell->maxcell; i++) {
      cell->minimum_width[i] =
          ceil_at_intervals(cell->minimum_width[i], rulewidth);
      cell->width[i] = ceil_at_intervals(cell->width[i], rulewidth);
      if (cell->fixed_width[i] > 0)
        cell->fixed_width[i] =
            ceil_at_intervals(cell->fixed_width[i], rulewidth);
    }
  }
  this->sloppy_width = this->fixed_table_width();
  if (this->total_width > this->sloppy_width)
    this->sloppy_width = this->total_width;
}

void table::check_minimum0(int min) {
  if (this->col < 0)
    return;
  if (this->tabwidth[this->col] < 0)
    return;
  this->check_row(this->row);
  int w = this->table_colspan(this->row, this->col);
  min += this->indent;
  int ww;
  if (w == 1)
    ww = min;
  else {
    auto cell = &this->cell;
    ww = 0;
    if (cell->icell >= 0 && cell->minimum_width[cell->icell] < min)
      cell->minimum_width[cell->icell] = min;
  }
  for (int i = this->col;
       i <= this->maxcol &&
       (i == this->col || (this->tabattr[this->row][i] & HTT_X));
       i++) {
    if (this->minimum_width[i] < ww)
      this->minimum_width[i] = ww;
  }
}

int table::setwidth0(struct table_mode *mode) {
  int w;
  int width = this->tabcontentssize;
  struct table_cell *cell = &this->cell;

  if (this->col < 0)
    return -1;
  if (this->tabwidth[this->col] < 0)
    return -1;
  this->check_row(this->row);
  if (this->linfo.prev_spaces > 0)
    width -= this->linfo.prev_spaces;
  w = this->table_colspan(this->row, this->col);
  if (w == 1) {
    if (this->tabwidth[this->col] < width)
      this->tabwidth[this->col] = width;
  } else if (cell->icell >= 0) {
    if (cell->width[cell->icell] < width)
      cell->width[cell->icell] = width;
  }
  return width;
}

void table::setwidth(struct table_mode *mode) {
  int width = this->setwidth0(mode);
  if (width < 0)
    return;
  if (this->tabattr[this->row][this->col] & HTT_NOWRAP)
    this->check_minimum0(width);
  if (mode->pre_mode & (TBLM_NOBR | TBLM_PRE | TBLM_PRE_INT) &&
      mode->nobr_offset >= 0)
    this->check_minimum0(width - mode->nobr_offset);
}

void table::addcontentssize(int width) {
  if (this->col < 0)
    return;
  if (this->tabwidth[this->col] < 0)
    return;
  this->check_row(this->row);
  this->tabcontentssize += width;
}

void table::clearcontentssize(struct table_mode *mode) {
  this->table_close_anchor0(mode);
  mode->nobr_offset = 0;
  this->linfo.prev_spaces = -1;
  set_space_to_prevchar(this->linfo.prevchar);
  this->linfo.prev_ctype = PC_ASCII;
  this->linfo.length = 0;
  this->tabcontentssize = 0;
}

void table::begin_cell(struct table_mode *mode) {
  this->clearcontentssize(mode);
  mode->indent_level = 0;
  mode->nobr_level = 0;
  mode->pre_mode = 0;
  this->indent = 0;
  this->flag |= TBL_IN_COL;

  if (this->suspended_data) {
    this->check_row(this->row);
    if (this->tabdata[this->row][this->col] == NULL)
      this->tabdata[this->row][this->col] =
          GeneralList<TextLine>::newGeneralList();
    this->tabdata[this->row][this->col]->appendGeneralList(
        this->suspended_data);
    this->suspended_data = NULL;
  }
}

void table::check_rowcol(table_mode *mode) {
  int row = this->row, col = this->col;

  if (!(this->flag & TBL_IN_ROW)) {
    this->flag |= TBL_IN_ROW;
    if (this->row + 1 < MAXROW_LIMIT)
      this->row++;
    if (this->row > this->maxrow)
      this->maxrow = this->row;
    this->col = -1;
  }
  if (this->row == -1)
    this->row = 0;
  if (this->col == -1)
    this->col = 0;

  for (;; this->row++) {
    this->check_row(this->row);
    for (; this->col < MAXCOL &&
           this->tabattr[this->row][this->col] & (HTT_X | HTT_Y);
         this->col++)
      ;
    if (this->col < MAXCOL)
      break;
    this->col = 0;
    if (this->row + 1 >= MAXROW_LIMIT)
      break;
  }
  if (this->row > this->maxrow)
    this->maxrow = this->row;
  if (this->col > this->maxcol)
    this->maxcol = this->col;

  if (this->row != row || this->col != col)
    this->begin_cell(mode);
  this->flag |= TBL_IN_COL;
}

int table::skip_space(const char *line, struct table_linfo *linfo,
                      int checkminimum) {
  int skip = 0, s = linfo->prev_spaces;
  Lineprop ctype, prev_ctype = linfo->prev_ctype;
  Str *prevchar = linfo->prevchar;
  int w = linfo->length;
  int min = 1;

  if (*line == '<' && line[strlen(line) - 1] == '>') {
    if (checkminimum)
      this->check_minimum0(visible_length(line));
    return 0;
  }

  while (*line) {
    const char *save = line, *c = line;
    int ec, len, wlen, plen;
    ctype = get_mctype(line);
    len = get_mcwidth(line);
    wlen = plen = get_mclen(line);

    if (min < w)
      min = w;
    if (ctype == PC_ASCII && IS_SPACE(*c)) {
      w = 0;
      s++;
    } else {
      if (*c == '&') {
        ec = getescapechar(&line);
        if (ec >= 0) {
          auto e = conv_entity(ec);
          ctype = get_mctype(e.c_str());
          len = get_strwidth(e.c_str());
          wlen = line - save;
          plen = get_mclen(e.c_str());
        }
      }
      if (prevchar->length &&
          is_boundary((unsigned char *)prevchar->ptr, (unsigned char *)c)) {
        w = len;
      } else {
        w += len;
      }
      if (s > 0) {
        skip += s - 1;
      }
      s = 0;
      prev_ctype = ctype;
    }
    set_prevchar(prevchar, c, plen);
    line = save + wlen;
  }
  if (s > 1) {
    skip += s - 1;
    linfo->prev_spaces = 1;
  } else {
    linfo->prev_spaces = s;
  }
  linfo->prev_ctype = prev_ctype;
  linfo->prevchar = prevchar;

  if (checkminimum) {
    if (min < w)
      min = w;
    linfo->length = w;
    this->check_minimum0(min);
  }
  return skip;
}

void table::feed_table_inline_tag(const char *line, struct table_mode *mode,
                                  int width) {
  this->check_rowcol(mode);
  this->pushdata(this->row, this->col, line);
  if (width >= 0) {
    this->check_minimum0(width);
    this->addcontentssize(width);
    this->setwidth(mode);
  }
}

void table::feed_table_block_tag(const char *line, struct table_mode *mode,
                                 int indent, int cmd) {
  int offset;
  if (mode->indent_level <= 0 && indent == -1)
    return;
  if (mode->indent_level >= CHAR_MAX && indent == 1)
    return;
  this->setwidth(mode);
  this->feed_table_inline_tag(line, mode, -1);
  this->clearcontentssize(mode);
  if (indent == 1) {
    mode->indent_level++;
    if (mode->indent_level <= MAX_INDENT_LEVEL)
      this->indent += INDENT_INCR;
  } else if (indent == -1) {
    mode->indent_level--;
    if (mode->indent_level < MAX_INDENT_LEVEL)
      this->indent -= INDENT_INCR;
  }
  if (this->indent < 0)
    this->indent = 0;
  offset = this->indent;
  if (cmd == HTML_DT) {
    if (mode->indent_level > 0 && mode->indent_level <= MAX_INDENT_LEVEL)
      offset -= INDENT_INCR;
    if (offset < 0)
      offset = 0;
  }
  if (this->indent > 0) {
    this->check_minimum0(0);
    this->addcontentssize(offset);
  }
}

void table::table_close_select(HtmlParser *parser, struct table_mode *mode,
                               int width) {
  Str *tmp = parser->process_n_select();
  mode->pre_mode &= ~TBLM_INSELECT;
  mode->end_tag = 0;
  this->feed_table1(parser, tmp, mode, width);
}

void table::table_close_textarea(HtmlParser *parser, struct table_mode *mode,
                                 int width) {
  Str *tmp = parser->process_n_textarea();
  mode->pre_mode &= ~TBLM_INTXTA;
  mode->end_tag = 0;
  this->feed_table1(parser, tmp, mode, width);
}

void table::table_close_anchor0(struct table_mode *mode) {
  if (!(mode->pre_mode & TBLM_ANCHOR))
    return;
  mode->pre_mode &= ~TBLM_ANCHOR;
  if (this->tabcontentssize == mode->anchor_offset) {
    this->check_minimum0(1);
    this->addcontentssize(1);
    this->setwidth(mode);
  } else if (this->linfo.prev_spaces > 0 &&
             this->tabcontentssize - 1 == mode->anchor_offset) {
    if (this->linfo.prev_spaces > 0)
      this->linfo.prev_spaces = -1;
  }
}

#define TAG_ACTION_NONE 0
#define TAG_ACTION_FEED 1
#define TAG_ACTION_TABLE 2
#define TAG_ACTION_N_TABLE 3
#define TAG_ACTION_PLAIN 4

#define CASE_TABLE_TAG                                                         \
  case HTML_TABLE:                                                             \
  case HTML_N_TABLE:                                                           \
  case HTML_TR:                                                                \
  case HTML_N_TR:                                                              \
  case HTML_TD:                                                                \
  case HTML_N_TD:                                                              \
  case HTML_TH:                                                                \
  case HTML_N_TH:                                                              \
  case HTML_THEAD:                                                             \
  case HTML_N_THEAD:                                                           \
  case HTML_TBODY:                                                             \
  case HTML_N_TBODY:                                                           \
  case HTML_TFOOT:                                                             \
  case HTML_N_TFOOT:                                                           \
  case HTML_COLGROUP:                                                          \
  case HTML_N_COLGROUP:                                                        \
  case HTML_COL

#define ATTR_ROWSPAN_MAX 32766

int table::feed_table_tag(HtmlParser *parser, const char *line,
                          struct table_mode *mode, int width,
                          const std::shared_ptr<HtmlTag> &tag) {
  int cmd;
  struct table_cell *cell = &this->cell;
  int colspan, rowspan;
  int col, prev_col;
  int i, j, k, v, v0, w, id;
  Str *tok, *tmp, *anchor;
  table_attr align, valign;

  cmd = tag->tagid;

  if (mode->pre_mode & TBLM_PLAIN) {
    if (mode->end_tag == cmd) {
      mode->pre_mode &= ~TBLM_PLAIN;
      mode->end_tag = 0;
      this->feed_table_block_tag(line, mode, 0, cmd);
      return TAG_ACTION_NONE;
    }
    return TAG_ACTION_PLAIN;
  }
  if (mode->pre_mode & TBLM_INTXTA) {
    switch (cmd) {
    CASE_TABLE_TAG:
    case HTML_N_TEXTAREA:
      this->table_close_textarea(parser, mode, width);
      if (cmd == HTML_N_TEXTAREA)
        return TAG_ACTION_NONE;
      break;
    default:
      return TAG_ACTION_FEED;
    }
  }
  if (mode->pre_mode & TBLM_SCRIPT) {
    if (mode->end_tag == cmd) {
      mode->pre_mode &= ~TBLM_SCRIPT;
      mode->end_tag = 0;
      return TAG_ACTION_NONE;
    }
    return TAG_ACTION_PLAIN;
  }
  if (mode->pre_mode & TBLM_STYLE) {
    if (mode->end_tag == cmd) {
      mode->pre_mode &= ~TBLM_STYLE;
      mode->end_tag = 0;
      return TAG_ACTION_NONE;
    }
    return TAG_ACTION_PLAIN;
  }
  /* failsafe: a tag other than <option></option>and </select> in *
   * <select> environment is regarded as the end of <select>. */
  if (mode->pre_mode & TBLM_INSELECT) {
    switch (cmd) {
    CASE_TABLE_TAG:
    case HTML_N_FORM:
    case HTML_N_SELECT: /* mode->end_tag */
      this->table_close_select(parser, mode, width);
      if (cmd == HTML_N_SELECT)
        return TAG_ACTION_NONE;
      break;
    default:
      return TAG_ACTION_FEED;
    }
  }
  if (mode->caption) {
    switch (cmd) {
    CASE_TABLE_TAG:
    case HTML_N_CAPTION:
      mode->caption = 0;
      if (cmd == HTML_N_CAPTION)
        return TAG_ACTION_NONE;
      break;
    default:
      return TAG_ACTION_FEED;
    }
  }

  if (mode->pre_mode & TBLM_PRE) {
    switch (cmd) {
    case HTML_NOBR:
    case HTML_N_NOBR:
    case HTML_PRE_INT:
    case HTML_N_PRE_INT:
      return TAG_ACTION_NONE;
    }
  }

  switch (cmd) {
  case HTML_TABLE:
    this->check_rowcol(mode);
    return TAG_ACTION_TABLE;
  case HTML_N_TABLE:
    if (this->suspended_data)
      this->check_rowcol(mode);
    return TAG_ACTION_N_TABLE;
  case HTML_TR:
    if (this->row + 1 >= MAXROW_LIMIT)
      return TAG_ACTION_NONE;
    if (this->col >= 0 && this->tabcontentssize > 0)
      this->setwidth(mode);
    this->col = -1;
    this->row++;
    this->flag |= TBL_IN_ROW;
    this->flag &= ~TBL_IN_COL;
    align = {};
    valign = {};
    if (tag->parsedtag_get_value(ATTR_ALIGN, &i)) {
      switch (i) {
      case ALIGN_LEFT:
        align = (HTT_LEFT | HTT_TRSET);
        break;
      case ALIGN_RIGHT:
        align = (HTT_RIGHT | HTT_TRSET);
        break;
      case ALIGN_CENTER:
        align = (HTT_CENTER | HTT_TRSET);
        break;
      }
    }
    if (tag->parsedtag_get_value(ATTR_VALIGN, &i)) {
      switch (i) {
      case VALIGN_TOP:
        valign = (HTT_TOP | HTT_VTRSET);
        break;
      case VALIGN_MIDDLE:
        valign = (HTT_MIDDLE | HTT_VTRSET);
        break;
      case VALIGN_BOTTOM:
        valign = (HTT_BOTTOM | HTT_VTRSET);
        break;
      }
    }
    this->trattr = align | valign;
    break;
  case HTML_TH:
  case HTML_TD:
    prev_col = this->col;
    if (this->col >= 0 && this->tabcontentssize > 0)
      this->setwidth(mode);
    if (this->row == -1) {
      /* for broken HTML... */
      this->row = -1;
      this->col = -1;
      this->maxrow = this->row;
    }
    if (this->col == -1) {
      if (!(this->flag & TBL_IN_ROW)) {
        if (this->row + 1 >= MAXROW_LIMIT)
          return TAG_ACTION_NONE;
        this->row++;
        this->flag |= TBL_IN_ROW;
      }
      if (this->row > this->maxrow)
        this->maxrow = this->row;
    }
    this->col++;
    this->check_row(this->row);
    while (this->col < MAXCOL && this->tabattr[this->row][this->col]) {
      this->col++;
    }
    if (this->col > MAXCOL - 1) {
      this->col = prev_col;
      return TAG_ACTION_NONE;
    }
    if (this->col > this->maxcol) {
      this->maxcol = this->col;
    }
    colspan = rowspan = 1;
    if (this->trattr & HTT_TRSET)
      align = (this->trattr & HTT_ALIGN);
    else if (cmd == HTML_TH)
      align = HTT_CENTER;
    else
      align = HTT_LEFT;
    if (this->trattr & HTT_VTRSET)
      valign = (this->trattr & HTT_VALIGN);
    else
      valign = HTT_MIDDLE;
    if (tag->parsedtag_get_value(ATTR_ROWSPAN, &rowspan)) {
      if (rowspan < 0 || rowspan > ATTR_ROWSPAN_MAX)
        rowspan = ATTR_ROWSPAN_MAX;
      if ((this->row + rowspan - 1) >= MAXROW_LIMIT)
        rowspan = MAXROW_LIMIT - this->row;
      if ((this->row + rowspan) > this->max_rowsize)
        this->check_row(this->row + rowspan - 1);
    }
    if (rowspan < 1)
      rowspan = 1;
    if (tag->parsedtag_get_value(ATTR_COLSPAN, &colspan)) {
      if ((this->col + colspan) >= MAXCOL) {
        /* Can't expand column */
        colspan = MAXCOL - this->col;
      }
    }
    if (colspan < 1)
      colspan = 1;
    if (tag->parsedtag_get_value(ATTR_ALIGN, &i)) {
      switch (i) {
      case ALIGN_LEFT:
        align = HTT_LEFT;
        break;
      case ALIGN_RIGHT:
        align = HTT_RIGHT;
        break;
      case ALIGN_CENTER:
        align = HTT_CENTER;
        break;
      }
    }
    if (tag->parsedtag_get_value(ATTR_VALIGN, &i)) {
      switch (i) {
      case VALIGN_TOP:
        valign = HTT_TOP;
        break;
      case VALIGN_MIDDLE:
        valign = HTT_MIDDLE;
        break;
      case VALIGN_BOTTOM:
        valign = HTT_BOTTOM;
        break;
      }
    }
#ifdef NOWRAP
    if (tag->parsedtag_exists(ATTR_NOWRAP))
      this->tabattr[this->row][this->col] |= HTT_NOWRAP;
#endif /* NOWRAP */
    v = 0;
    if (tag->parsedtag_get_value(ATTR_WIDTH, &v)) {
      v = RELATIVE_WIDTH(v);
    }
#ifdef NOWRAP
    if (v != 0) {
      /* NOWRAP and WIDTH= conflicts each other */
      this->tabattr[this->row][this->col] &= ~HTT_NOWRAP;
    }
#endif /* NOWRAP */
    this->tabattr[this->row][this->col] &= ~(HTT_ALIGN | HTT_VALIGN);
    this->tabattr[this->row][this->col] |= (align | valign);
    if (colspan > 1) {
      col = this->col;

      cell->icell = cell->maxcell + 1;
      k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
                         cell->index, cell->icell);
      if (k <= cell->maxcell) {
        i = cell->index[k];
        if (cell->col[i] == col && cell->colspan[i] == colspan)
          cell->icell = i;
      }
      if (cell->icell > cell->maxcell && cell->icell < MAXCELL) {
        cell->maxcell++;
        cell->col[cell->maxcell] = col;
        cell->colspan[cell->maxcell] = colspan;
        cell->width[cell->maxcell] = 0;
        cell->minimum_width[cell->maxcell] = 0;
        cell->fixed_width[cell->maxcell] = 0;
        if (cell->maxcell > k) {
          int ii;
          for (ii = cell->maxcell; ii > k; ii--)
            cell->index[ii] = cell->index[ii - 1];
        }
        cell->index[k] = cell->maxcell;
      }
      if (cell->icell > cell->maxcell)
        cell->icell = -1;
    }
    if (v != 0) {
      if (colspan == 1) {
        v0 = this->fixed_width[this->col];
        if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0)) {
#ifdef FEED_TABLE_DEBUG
          fprintf(stderr, "width(%d) = %d\n", this->col, v);
#endif /* TABLE_DEBUG */
          this->fixed_width[this->col] = v;
        }
      } else if (cell->icell >= 0) {
        v0 = cell->fixed_width[cell->icell];
        if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0))
          cell->fixed_width[cell->icell] = v;
      }
    }
    for (i = 0; i < rowspan; i++) {
      this->check_row(this->row + i);
      for (j = 0; j < colspan; j++) {
        if (!(this->tabattr[this->row + i][this->col + j] & (HTT_X | HTT_Y))) {
          this->tabattr[this->row + i][this->col + j] |=
              static_cast<table_attr>(((i > 0) ? HTT_Y : 0) |
                                      ((j > 0) ? HTT_X : 0));
        }
        if (this->col + j > this->maxcol) {
          this->maxcol = this->col + j;
        }
      }
      if (this->row + i > this->maxrow) {
        this->maxrow = this->row + i;
      }
    }
    this->begin_cell(mode);
    break;
  case HTML_N_TR:
    this->setwidth(mode);
    this->col = -1;
    this->flag &= ~(TBL_IN_ROW | TBL_IN_COL);
    return TAG_ACTION_NONE;
  case HTML_N_TH:
  case HTML_N_TD:
    this->setwidth(mode);
    this->flag &= ~TBL_IN_COL;
#ifdef FEED_TABLE_DEBUG
    {
      TextListItem *it;
      int i = this->col, j = this->row;
      fprintf(stderr, "(a) row,col: %d, %d\n", j, i);
      if (this->tabdata[j] && this->tabdata[j][i]) {
        for (it = ((TextList *)this->tabdata[j][i])->first; it; it = it->next)
          fprintf(stderr, "  [%s] \n", it->ptr);
      }
    }
#endif
    return TAG_ACTION_NONE;
  case HTML_P:
  case HTML_BR:
  case HTML_CENTER:
  case HTML_N_CENTER:
  case HTML_DIV:
  case HTML_N_DIV:
    if (!(this->flag & TBL_IN_ROW))
      break;
  case HTML_DT:
  case HTML_DD:
  case HTML_H:
  case HTML_N_H:
  case HTML_LI:
  case HTML_PRE:
  case HTML_N_PRE:
  case HTML_HR:
  case HTML_LISTING:
  case HTML_XMP:
  case HTML_PLAINTEXT:
  case HTML_PRE_PLAIN:
  case HTML_N_PRE_PLAIN:
    this->feed_table_block_tag(line, mode, 0, cmd);
    switch (cmd) {
    case HTML_PRE:
    case HTML_PRE_PLAIN:
      mode->pre_mode |= TBLM_PRE;
      break;
    case HTML_N_PRE:
    case HTML_N_PRE_PLAIN:
      mode->pre_mode &= ~TBLM_PRE;
      break;
    case HTML_LISTING:
      mode->pre_mode |= TBLM_PLAIN;
      mode->end_tag = HTML_N_LISTING;
      break;
    case HTML_XMP:
      mode->pre_mode |= TBLM_PLAIN;
      mode->end_tag = HTML_N_XMP;
      break;
    case HTML_PLAINTEXT:
      mode->pre_mode |= TBLM_PLAIN;
      mode->end_tag = MAX_HTMLTAG;
      break;
    }
    break;
  case HTML_DL:
  case HTML_BLQ:
  case HTML_OL:
  case HTML_UL:
    this->feed_table_block_tag(line, mode, 1, cmd);
    break;
  case HTML_N_DL:
  case HTML_N_BLQ:
  case HTML_N_OL:
  case HTML_N_UL:
    this->feed_table_block_tag(line, mode, -1, cmd);
    break;
  case HTML_NOBR:
  case HTML_WBR:
    if (!(this->flag & TBL_IN_ROW))
      break;
  case HTML_PRE_INT:
    this->feed_table_inline_tag(line, mode, -1);
    switch (cmd) {
    case HTML_NOBR:
      mode->nobr_level++;
      if (mode->pre_mode & TBLM_NOBR)
        return TAG_ACTION_NONE;
      mode->pre_mode |= TBLM_NOBR;
      break;
    case HTML_PRE_INT:
      if (mode->pre_mode & TBLM_PRE_INT)
        return TAG_ACTION_NONE;
      mode->pre_mode |= TBLM_PRE_INT;
      this->linfo.prev_spaces = 0;
      break;
    }
    mode->nobr_offset = -1;
    if (this->linfo.length > 0) {
      this->check_minimum0(this->linfo.length);
      this->linfo.length = 0;
    }
    break;
  case HTML_N_NOBR:
    if (!(this->flag & TBL_IN_ROW))
      break;
    this->feed_table_inline_tag(line, mode, -1);
    if (mode->nobr_level > 0)
      mode->nobr_level--;
    if (mode->nobr_level == 0)
      mode->pre_mode &= ~TBLM_NOBR;
    break;
  case HTML_N_PRE_INT:
    this->feed_table_inline_tag(line, mode, -1);
    mode->pre_mode &= ~TBLM_PRE_INT;
    break;
  case HTML_IMG:
    this->check_rowcol(mode);
    w = this->fixed_width[this->col];
    if (w < 0) {
      if (this->total_width > 0)
        w = -this->total_width * w / 100;
      else if (width > 0)
        w = -width * w / 100;
      else
        w = 0;
    } else if (w == 0) {
      if (this->total_width > 0)
        w = this->total_width;
      else if (width > 0)
        w = width;
    }
    tok = parser->process_img(tag, w);
    this->feed_table1(parser, tok, mode, width);
    break;
  case HTML_FORM:
    this->feed_table_block_tag("", mode, 0, cmd);
    tmp = parser->process_form(tag);
    if (tmp)
      this->feed_table1(parser, tmp, mode, width);
    break;
  case HTML_N_FORM:
    this->feed_table_block_tag("", mode, 0, cmd);
    parser->process_n_form();
    break;
  case HTML_INPUT:
    tmp = parser->process_input(tag);
    this->feed_table1(parser, tmp, mode, width);
    break;
  case HTML_BUTTON:
    tmp = parser->process_button(tag);
    this->feed_table1(parser, tmp, mode, width);
    break;
  case HTML_N_BUTTON:
    tmp = parser->process_n_button();
    this->feed_table1(parser, tmp, mode, width);
    break;
  case HTML_SELECT:
    tmp = parser->process_select(tag);
    if (tmp)
      this->feed_table1(parser, tmp, mode, width);
    mode->pre_mode |= TBLM_INSELECT;
    mode->end_tag = HTML_N_SELECT;
    break;
  case HTML_N_SELECT:
  case HTML_OPTION:
    /* nothing */
    break;
  case HTML_TEXTAREA:
    w = 0;
    this->check_rowcol(mode);
    if (this->col + 1 <= this->maxcol &&
        this->tabattr[this->row][this->col + 1] & HTT_X) {
      if (cell->icell >= 0 && cell->fixed_width[cell->icell] > 0)
        w = cell->fixed_width[cell->icell];
    } else {
      if (this->fixed_width[this->col] > 0)
        w = this->fixed_width[this->col];
    }
    tmp = parser->process_textarea(tag, w);
    if (tmp)
      this->feed_table1(parser, tmp, mode, width);
    mode->pre_mode |= TBLM_INTXTA;
    mode->end_tag = HTML_N_TEXTAREA;
    break;
  case HTML_A:
    this->table_close_anchor0(mode);
    anchor = NULL;
    i = 0;
    tag->parsedtag_get_value(ATTR_HREF, &anchor);
    tag->parsedtag_get_value(ATTR_HSEQ, &i);
    if (anchor) {
      this->check_rowcol(mode);
      if (i == 0) {
        Str *tmp = parser->process_anchor(tag, line);
        if (displayLinkNumber) {
          Str *t = parser->getLinkNumberStr(-1);
          this->feed_table_inline_tag(NULL, mode, t->length);
          Strcat(tmp, t);
        }
        this->pushdata(this->row, this->col, tmp->ptr);
      } else
        this->pushdata(this->row, this->col, line);
      if (i >= 0) {
        mode->pre_mode |= TBLM_ANCHOR;
        mode->anchor_offset = this->tabcontentssize;
      }
    } else
      this->suspend_or_pushdata(line);
    break;
  case HTML_DEL:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      mode->pre_mode |= TBLM_DEL;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      this->feed_table_inline_tag(line, mode, 5); /* [DEL: */
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      this->feed_table_inline_tag(line, mode, -1);
      break;
    }
    break;
  case HTML_N_DEL:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      mode->pre_mode &= ~TBLM_DEL;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      this->feed_table_inline_tag(line, mode, 5); /* :DEL] */
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      this->feed_table_inline_tag(line, mode, -1);
      break;
    }
    break;
  case HTML_S:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      mode->pre_mode |= TBLM_S;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      this->feed_table_inline_tag(line, mode, 3); /* [S: */
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      this->feed_table_inline_tag(line, mode, -1);
      break;
    }
    break;
  case HTML_N_S:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      mode->pre_mode &= ~TBLM_S;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      this->feed_table_inline_tag(line, mode, 3); /* :S] */
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      this->feed_table_inline_tag(line, mode, -1);
      break;
    }
    break;
  case HTML_INS:
  case HTML_N_INS:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      break;
    case DISPLAY_INS_DEL_NORMAL:
      this->feed_table_inline_tag(line, mode, 5); /* [INS:, :INS] */
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      this->feed_table_inline_tag(line, mode, -1);
      break;
    }
    break;
  case HTML_SUP:
  case HTML_SUB:
  case HTML_N_SUB:
    if (!(mode->pre_mode & (TBLM_DEL | TBLM_S)))
      this->feed_table_inline_tag(line, mode, 1); /* ^, [, ] */
    break;
  case HTML_N_SUP:
    break;
  case HTML_TABLE_ALT:
    id = -1;
    tag->parsedtag_get_value(ATTR_TID, &id);
    if (id >= 0 && id < this->ntable) {
      struct table *tbl1 = this->tables[id].ptr;
      this->feed_table_block_tag(line, mode, 0, cmd);
      this->addcontentssize(tbl1->maximum_table_width());
      this->check_minimum0(tbl1->sloppy_width);
      this->setwidth0(mode);
      this->clearcontentssize(mode);
    }
    break;
  case HTML_CAPTION:
    mode->caption = 1;
    break;
  case HTML_N_CAPTION:
  case HTML_THEAD:
  case HTML_N_THEAD:
  case HTML_TBODY:
  case HTML_N_TBODY:
  case HTML_TFOOT:
  case HTML_N_TFOOT:
  case HTML_COLGROUP:
  case HTML_N_COLGROUP:
  case HTML_COL:
    break;
  case HTML_SCRIPT:
    mode->pre_mode |= TBLM_SCRIPT;
    mode->end_tag = HTML_N_SCRIPT;
    break;
  case HTML_STYLE:
    mode->pre_mode |= TBLM_STYLE;
    mode->end_tag = HTML_N_STYLE;
    break;
  case HTML_N_A:
    this->table_close_anchor0(mode);
  case HTML_FONT:
  case HTML_N_FONT:
  case HTML_NOP:
    this->suspend_or_pushdata(line);
    break;
  case HTML_INTERNAL:
  case HTML_N_INTERNAL:
  case HTML_FORM_INT:
  case HTML_N_FORM_INT:
  case HTML_INPUT_ALT:
  case HTML_N_INPUT_ALT:
  case HTML_SELECT_INT:
  case HTML_N_SELECT_INT:
  case HTML_OPTION_INT:
  case HTML_TEXTAREA_INT:
  case HTML_N_TEXTAREA_INT:
  case HTML_IMG_ALT:
  case HTML_SYMBOL:
  case HTML_N_SYMBOL:
  default:
    /* unknown tag: put into table */
    return TAG_ACTION_FEED;
  }
  return TAG_ACTION_NONE;
}

int table::feed_table(HtmlParser *parser, const char *line,
                      struct table_mode *mode, int width, int internal) {
  int i;
  const char *p;
  Str *tmp;
  struct table_linfo *linfo = &this->linfo;

  if (*line == '<' && line[1] && REALLY_THE_BEGINNING_OF_A_TAG(line)) {
    p = line;
    auto tag = HtmlTag::parse(&p, internal);
    if (tag) {
      switch (this->feed_table_tag(parser, line, mode, width, tag)) {
      case TAG_ACTION_NONE:
        return -1;
      case TAG_ACTION_N_TABLE:
        return 0;
      case TAG_ACTION_TABLE:
        return 1;
      case TAG_ACTION_PLAIN:
        break;
      case TAG_ACTION_FEED:
      default:
        if (tag->parsedtag_need_reconstruct()) {
          line = tag->parsedtag2str()->ptr;
        }
      }
    } else {
      if (!(mode->pre_mode & (TBLM_PLAIN | TBLM_INTXTA | TBLM_INSELECT |
                              TBLM_SCRIPT | TBLM_STYLE)))
        return -1;
    }
  } else {
    if (mode->pre_mode & (TBLM_DEL | TBLM_S))
      return -1;
  }
  if (mode->caption) {
    Strcat_charp(this->caption, line);
    return -1;
  }
  if (mode->pre_mode & TBLM_SCRIPT)
    return -1;
  if (mode->pre_mode & TBLM_STYLE)
    return -1;
  if (mode->pre_mode & TBLM_INTXTA) {
    parser->feed_textarea(line);
    return -1;
  }
  if (mode->pre_mode & TBLM_INSELECT) {
    parser->feed_select(line);
    return -1;
  }
  if (!(mode->pre_mode & TBLM_PLAIN) &&
      !(*line == '<' && line[strlen(line) - 1] == '>') &&
      strchr(line, '&') != NULL) {
    tmp = Strnew();
    for (p = line; *p;) {
      const char *q;
      if (*p == '&') {
        if (!strncasecmp(p, "&amp;", 5) || !strncasecmp(p, "&gt;", 4) ||
            !strncasecmp(p, "&lt;", 4)) {
          /* do not convert */
          Strcat_char(tmp, *p);
          p++;
        } else {
          int ec;
          q = p;
          switch (ec = getescapechar(&p)) {
          case '<':
            Strcat_charp(tmp, "&lt;");
            break;
          case '>':
            Strcat_charp(tmp, "&gt;");
            break;
          case '&':
            Strcat_charp(tmp, "&amp;");
            break;
          case '\r':
            Strcat_char(tmp, '\n');
            break;
          default: {
            auto e = conv_entity(ec);
            if (e.empty())
              break;
            if (e.size() == 1 && ec == e[0]) {
              Strcat_char(tmp, e[0]);
              break;
            }
          }

          case -1:
            Strcat_char(tmp, *q);
            p = q + 1;
            break;
          }
        }
      } else {
        Strcat_char(tmp, *p);
        p++;
      }
    }
    line = tmp->ptr;
  }
  if (!(mode->pre_mode & (TBLM_SPECIAL & ~TBLM_NOBR))) {
    if (!(this->flag & TBL_IN_COL) || linfo->prev_spaces != 0)
      while (IS_SPACE(*line))
        line++;
    if (*line == '\0')
      return -1;
    this->check_rowcol(mode);
    if (mode->pre_mode & TBLM_NOBR && mode->nobr_offset < 0)
      mode->nobr_offset = this->tabcontentssize;

    /* count of number of spaces skipped in normal mode */
    i = this->skip_space(line, linfo, !(mode->pre_mode & TBLM_NOBR));
    this->addcontentssize(visible_length(line) - i);
    this->setwidth(mode);
    this->pushdata(this->row, this->col, line);
  } else if (mode->pre_mode & TBLM_PRE_INT) {
    this->check_rowcol(mode);
    if (mode->nobr_offset < 0)
      mode->nobr_offset = this->tabcontentssize;
    this->addcontentssize(maximum_visible_length(line, this->tabcontentssize));
    this->setwidth(mode);
    this->pushdata(this->row, this->col, line);
  } else {
    /* <pre> mode or something like it */
    this->check_rowcol(mode);
    while (*line) {
      int nl = false;
      if ((p = strchr(line, '\r')) || (p = strchr(line, '\n'))) {
        if (*p == '\r' && p[1] == '\n')
          p++;
        if (p[1]) {
          p++;
          tmp = Strnew_charp_n(line, p - line);
          line = p;
          p = tmp->ptr;
        } else {
          p = line;
          line = "";
        }
        nl = true;
      } else {
        p = line;
        line = "";
      }
      if (mode->pre_mode & TBLM_PLAIN)
        i = maximum_visible_length_plain(p, this->tabcontentssize);
      else
        i = maximum_visible_length(p, this->tabcontentssize);
      this->addcontentssize(i);
      this->setwidth(mode);
      if (nl)
        this->clearcontentssize(mode);
      this->pushdata(this->row, this->col, p);
    }
  }
  return -1;
}

void table::feed_table1(HtmlParser *parser, Str *tok, struct table_mode *mode,
                        int width) {
  Str *tokbuf;
  ReadBufferStatus status;
  if (!tok)
    return;
  tokbuf = Strnew();
  status = R_ST_NORMAL;
  auto line = tok->ptr;
  while (read_token(tokbuf, (const char **)&line, &status,
                    mode->pre_mode & TBLM_PREMODE, 0))
    this->feed_table(parser, tokbuf->ptr, mode, width, true);
}

void table::pushTable(struct table *tbl1) {
  int col;
  int row;

  if (this->ntable < 0 || this->ntable >= MAX_TABLE_N_LIMIT)
    return;
  col = this->col;
  row = this->row;

  if (this->ntable >= this->tables_size) {
    struct table_in *tmp;
    this->tables_size += MAX_TABLE_N;
    if (this->tables_size <= 0 || this->tables_size > MAX_TABLE_N_LIMIT)
      this->tables_size = MAX_TABLE_N_LIMIT;
    tmp = (struct table_in *)New_N(struct table_in, this->tables_size);
    if (this->tables)
      memcpy(tmp, this->tables, this->ntable * sizeof(struct table_in));
    this->tables = tmp;
  }

  this->tables[this->ntable].ptr = tbl1;
  this->tables[this->ntable].col = col;
  this->tables[this->ntable].row = row;
  this->tables[this->ntable].indent = this->indent;
  this->tables[this->ntable].buf = GeneralList<TextLine>::newGeneralList();
  this->check_row(row);
  if (col + 1 <= this->maxcol && this->tabattr[row][col + 1] & HTT_X)
    this->tables[this->ntable].cell = this->cell.icell;
  else
    this->tables[this->ntable].cell = -1;
  this->ntable++;
}

int table::correct_table_matrix(int col, int cspan, int a, double b) {
  int ecol = col + cspan;
  double w = 1. / (b * b);

  int i = col;
  for (; i < ecol; i++) {
    this->vector.v_add_val(i, w * a);
    for (int j = i; j < ecol; j++) {
      this->matrix.m_add_val(i, j, w);
      this->matrix.m_set_val(j, i, this->matrix.m_entry(i, j));
    }
  }
  return i;
}

void table::correct_table_matrix2(int col, int cspan, double s, double b) {
  int i, j;
  int ecol = col + cspan;
  int size = this->maxcol + 1;
  double w = 1. / (b * b);
  double ss;

  for (i = 0; i < size; i++) {
    for (j = i; j < size; j++) {
      if (i >= col && i < ecol && j >= col && j < ecol)
        ss = (1. - s) * (1. - s);
      else if ((i >= col && i < ecol) || (j >= col && j < ecol))
        ss = -(1. - s) * s;
      else
        ss = s * s;
      this->matrix.m_add_val(i, j, w * ss);
    }
  }
}

void table::correct_table_matrix3(int col, char *flags, double s, double b) {
  int i, j;
  double ss;
  int size = this->maxcol + 1;
  double w = 1. / (b * b);
  int flg = (flags[col] == 0);

  for (i = 0; i < size; i++) {
    if (!((flg && flags[i] == 0) || (!flg && flags[i] != 0)))
      continue;
    for (j = i; j < size; j++) {
      if (!((flg && flags[j] == 0) || (!flg && flags[j] != 0)))
        continue;
      if (i == col && j == col)
        ss = (1. - s) * (1. - s);
      else if (i == col || j == col)
        ss = -(1. - s) * s;
      else
        ss = s * s;
      this->matrix.m_add_val(i, j, w * ss);
    }
  }
}

void table::correct_table_matrix4(int col, int cspan, char *flags, double s,
                                  double b) {
  int i, j;
  double ss;
  int ecol = col + cspan;
  int size = this->maxcol + 1;
  double w = 1. / (b * b);

  for (i = 0; i < size; i++) {
    if (flags[i] && !(i >= col && i < ecol))
      continue;
    for (j = i; j < size; j++) {
      if (flags[j] && !(j >= col && j < ecol))
        continue;
      if (i >= col && i < ecol && j >= col && j < ecol)
        ss = (1. - s) * (1. - s);
      else if ((i >= col && i < ecol) || (j >= col && j < ecol))
        ss = -(1. - s) * s;
      else
        ss = s * s;
      this->matrix.m_add_val(i, j, w * ss);
    }
  }
}

void table::set_table_matrix0(int maxwidth) {
  size_t size = this->maxcol + 1;
  int j, k, bcol;
  int width;
  double w0, w1, s, b;
#ifdef __GNUC__
  double we[size];
  char expand[size];
#else  /* not __GNUC__ */
  double we[MAXCOL];
  char expand[MAXCOL] = {0};
#endif /* not __GNUC__ */
  struct table_cell *cell = &this->cell;

  w0 = 0.;
  for (size_t i = 0; i < size; i++) {
    we[i] = weight(this->tabwidth[i]);
    w0 += we[i];
  }
  if (w0 <= 0.)
    w0 = 1.;

  if (cell->necell == 0) {
    for (size_t i = 0; i < size; i++) {
      s = we[i] / w0;
      b = sigma_td_nw((int)(s * maxwidth));
      this->correct_table_matrix2(i, 1, s, b);
    }
    return;
  }

  for (k = 0; k < cell->necell; k++) {
    j = cell->eindex[k];
    bcol = cell->col[j];
    size_t ecol = bcol + cell->colspan[j];
    width = cell->width[j] - (cell->colspan[j] - 1) * this->cellspacing;
    w1 = 0.;
    for (size_t i = bcol; i < ecol; i++) {
      w1 += this->tabwidth[i] + 0.1;
      expand[i]++;
    }
    for (size_t i = bcol; i < ecol; i++) {
      auto w = weight(static_cast<int>(width * (this->tabwidth[i] + 0.1) / w1));
      if (w > we[i])
        we[i] = w;
    }
  }

  w0 = 0.;
  w1 = 0.;
  for (size_t i = 0; i < size; i++) {
    w0 += we[i];
    if (expand[i] == 0)
      w1 += we[i];
  }
  if (w0 <= 0.)
    w0 = 1.;

  for (k = 0; k < cell->necell; k++) {
    j = cell->eindex[k];
    bcol = cell->col[j];
    width = cell->width[j] - (cell->colspan[j] - 1) * this->cellspacing;
    auto w = weight(width);
    s = w / (w1 + w);
    b = sigma_td_nw((int)(s * maxwidth));
    this->correct_table_matrix4(bcol, cell->colspan[j], expand, s, b);
  }

  for (size_t i = 0; i < size; i++) {
    if (expand[i] == 0) {
      s = we[i] / max(w1, 1.);
      b = sigma_td_nw((int)(s * maxwidth));
    } else {
      s = we[i] / max(w0 - w1, 1.);
      b = sigma_td_nw(maxwidth);
    }
    this->correct_table_matrix3(i, expand, s, b);
  }
}

void table::check_relative_width(int maxwidth) {
  int i;
  double rel_total = 0;
  int size = this->maxcol + 1;
  double *rcolwidth = (double *)New_N(double, size);
  struct table_cell *cell = &this->cell;
  int n_leftcol = 0;

  for (i = 0; i < size; i++)
    rcolwidth[i] = 0;

  for (i = 0; i < size; i++) {
    if (this->fixed_width[i] < 0)
      rcolwidth[i] = -(double)this->fixed_width[i] / 100.0;
    else if (this->fixed_width[i] > 0)
      rcolwidth[i] = (double)this->fixed_width[i] / maxwidth;
    else
      n_leftcol++;
  }
  for (i = 0; i <= cell->maxcell; i++) {
    if (cell->fixed_width[i] < 0) {
      double w = -(double)cell->fixed_width[i] / 100.0;
      double r;
      int j, k;
      int n_leftcell = 0;
      k = cell->col[i];
      r = 0.0;
      for (j = 0; j < cell->colspan[i]; j++) {
        if (rcolwidth[j + k] > 0)
          r += rcolwidth[j + k];
        else
          n_leftcell++;
      }
      if (n_leftcell == 0) {
        /* w must be identical to r */
        if (w != r) {
          cell->fixed_width[i] = static_cast<short>(-100 * r);
        }
      } else {
        if (w <= r) {
          /* make room for the left(width-unspecified) cell */
          /* the next formula is an estimation of required width */
          w = r * cell->colspan[i] / (cell->colspan[i] - n_leftcell);
          cell->fixed_width[i] = static_cast<short>(-100 * w);
        }
        for (j = 0; j < cell->colspan[i]; j++) {
          if (rcolwidth[j + k] == 0)
            rcolwidth[j + k] = (w - r) / n_leftcell;
        }
      }
    } else if (cell->fixed_width[i] > 0) {
      /* todo */
    }
  }
  /* sanity check */
  for (i = 0; i < size; i++)
    rel_total += rcolwidth[i];

  if ((n_leftcol == 0 && rel_total < 0.9) || 1.1 < rel_total) {
    for (i = 0; i < size; i++) {
      rcolwidth[i] /= rel_total;
    }
    for (i = 0; i < size; i++) {
      if (this->fixed_width[i] < 0)
        this->fixed_width[i] = static_cast<short>(-rcolwidth[i] * 100);
    }
    for (i = 0; i <= cell->maxcell; i++) {
      if (cell->fixed_width[i] < 0) {
        double r;
        int j, k;
        k = cell->col[i];
        r = 0.0;
        for (j = 0; j < cell->colspan[i]; j++)
          r += rcolwidth[j + k];
        cell->fixed_width[i] = static_cast<short>(-r * 100);
      }
    }
  }
}

void table::set_table_matrix(int width) {
  int size = this->maxcol + 1;
  if (size < 1)
    return;

  this->matrix = Matrix(size);
  this->vector = Vector(size);
  for (int i = 0; i < size; i++) {
    for (int j = i; j < size; j++)
      this->matrix.m_set_val(i, j, 0.);
    this->vector.v_set_val(i, 0.);
  }

  this->check_relative_width(width);

  for (int i = 0; i < size; i++) {
    if (this->fixed_width[i] > 0) {
      auto a = max(this->fixed_width[i], this->minimum_width[i]);
      auto b = sigma_td(a);
      this->correct_table_matrix(i, 1, a, b);
    } else if (this->fixed_width[i] < 0) {
      auto s = -(double)this->fixed_width[i] / 100.;
      auto b = sigma_td((int)(s * width));
      this->correct_table_matrix2(i, 1, s, b);
    }
  }

  auto cell = &this->cell;
  for (int j = 0; j <= cell->maxcell; j++) {
    if (cell->fixed_width[j] > 0) {
      auto a = max(cell->fixed_width[j], cell->minimum_width[j]);
      auto b = sigma_td(a);
      this->correct_table_matrix(cell->col[j], cell->colspan[j], a, b);
    } else if (cell->fixed_width[j] < 0) {
      auto s = -(double)cell->fixed_width[j] / 100.;
      auto b = sigma_td((int)(s * width));
      this->correct_table_matrix2(cell->col[j], cell->colspan[j], s, b);
    }
  }

  this->set_table_matrix0(width);

  double b;
  if (this->total_width > 0) {
    b = sigma_table(width);
  } else {
    b = sigma_table_nw(width);
  }
  this->correct_table_matrix(0, size, width, b);
}
