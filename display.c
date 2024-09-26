#include "fm.h"
#include "tabbuffer.h"
#include "buffer.h"
#include "scr.h"
#include "termsize.h"
#include "terms.h"

#define EFFECT_ANCHOR_START scr_underline()
#define EFFECT_ANCHOR_END scr_underlineend()
#define EFFECT_IMAGE_START scr_standout()
#define EFFECT_IMAGE_END scr_standend()
#define EFFECT_FORM_START scr_standout()
#define EFFECT_FORM_END scr_standend()
#define EFFECT_ACTIVE_START scr_bold()
#define EFFECT_ACTIVE_END scr_boldend()
#define EFFECT_VISITED_START /**/
#define EFFECT_VISITED_END   /**/
#define EFFECT_MARK_START scr_standout()
#define EFFECT_MARK_END scr_standend()

/*
 * Display some lines.
 */
static Line *cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;

static struct Buffer *save_current_buf = NULL;

static void drawAnchorCursor(struct Buffer *buf);
#define redrawBuffer(buf) redrawNLine(buf, LASTLINE)
static void redrawNLine(struct Buffer *buf, int n);
static Line *redrawLine(struct Buffer *buf, Line *l, int i);
static int redrawLineRegion(struct Buffer *buf, Line *l, int i, int bpos,
                            int epos);
static void do_effects(Lineprop m);

static Str make_lastline_link(struct Buffer *buf, char *title, char *url) {
  Str s = NULL, u;
  struct Url pu;
  char *p;
  int l = COLS - 1, i;

  if (title && *title) {
    s = Strnew_m_charp("[", title, "]", NULL);
    for (p = s->ptr; *p; p++) {
      if (IS_CNTRL(*p) || IS_SPACE(*p))
        *p = ' ';
    }
    if (url)
      Strcat_charp(s, " ");
    l -= get_Str_strwidth(s);
    if (l <= 0)
      return s;
  }
  if (!url)
    return s;
  parseURL2(url, &pu, baseURL(buf));
  u = parsedURL2Str(&pu);
  if (DecodeURL)
    u = Strnew_charp(url_decode2(u->ptr, buf));
  if (l <= 4 || l >= get_Str_strwidth(u)) {
    if (!s)
      return u;
    Strcat(s, u);
    return s;
  }
  if (!s)
    s = Strnew_size(COLS);
  i = (l - 2) / 2;
  Strcat_charp_n(s, u->ptr, i);
  Strcat_charp(s, "..");
  i = get_Str_strwidth(u) - (COLS - 1 - get_Str_strwidth(s));
  Strcat_charp(s, &u->ptr[i]);
  return s;
}

static Str make_lastline_message(struct Buffer *buf) {
  Str msg, s = NULL;
  int sl = 0;

  if (displayLink) {
    {
      Anchor *a = retrieveCurrentAnchor(buf);
      char *p = NULL;
      if (a && a->title && *a->title)
        p = a->title;
      else {
        Anchor *a_img = retrieveCurrentImg(buf);
        if (a_img && a_img->title && *a_img->title)
          p = a_img->title;
      }
      if (p || a)
        s = make_lastline_link(buf, p, a ? a->url : NULL);
    }
    if (s) {
      sl = get_Str_strwidth(s);
      if (sl >= COLS - 3)
        return s;
    }
  }

  msg = Strnew();
  if (displayLineInfo && buf->currentLine != NULL && buf->lastLine != NULL) {
    int cl = buf->currentLine->real_linenumber;
    int ll = buf->lastLine->real_linenumber;
    int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
    Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
  } else
    /* FIXME: gettextize? */
    Strcat_charp(msg, "Viewing");
  if (buf->ssl_certificate)
    Strcat_charp(msg, "[SSL]");
  Strcat_charp(msg, " <");
  Strcat_charp(msg, buf->buffername);

  if (s) {
    int l = COLS - 3 - sl;
    if (get_Str_strwidth(msg) > l) {
      Strtruncate(msg, l);
    }
    Strcat_charp(msg, "> ");
    Strcat(msg, s);
  } else {
    Strcat_charp(msg, ">");
  }
  return msg;
}

void displayBuffer(struct Buffer *buf, int mode) {
  Str msg;
  int ny = 0;

  if (!buf)
    return;
  if (buf->topLine == NULL && readBufferCache(buf) == 0) { /* clear_buffer */
    mode = B_FORCE_REDRAW;
  }

  if (buf->width == 0)
    buf->width = INIT_BUFFER_WIDTH;
  if (buf->height == 0)
    buf->height = LASTLINE + 1;
  if ((buf->width != INIT_BUFFER_WIDTH &&
       (is_html_type(buf->type) || FoldLine)) ||
      buf->need_reshape) {
    buf->need_reshape = TRUE;
    reshapeBuffer(buf);
  }
  if (showLineNum) {
    if (buf->lastLine && buf->lastLine->real_linenumber > 0)
      buf->rootX =
          (int)(log(buf->lastLine->real_linenumber + 0.1) / log(10)) + 2;
    if (buf->rootX < 5)
      buf->rootX = 5;
    if (buf->rootX > COLS)
      buf->rootX = COLS;
  } else
    buf->rootX = 0;
  buf->COLS = COLS - buf->rootX;
  if (nTab > 1) {
    if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
      calcTabPos();
    ny = LastTab->y + 2;
    if (ny > LASTLINE)
      ny = LASTLINE;
  }
  if (buf->rootY != ny || buf->LINES != LASTLINE - ny) {
    buf->rootY = ny;
    buf->LINES = LASTLINE - ny;
    arrangeCursor(buf);
    mode = B_REDRAW_IMAGE;
  }
  if (mode == B_FORCE_REDRAW || mode == B_SCROLL || mode == B_REDRAW_IMAGE ||
      cline != buf->topLine || ccolumn != buf->currentColumn) {
    {
      redrawBuffer(buf);
    }
    cline = buf->topLine;
    ccolumn = buf->currentColumn;
  }
  if (buf->topLine == NULL)
    buf->topLine = buf->firstLine;

  drawAnchorCursor(buf);

  msg = make_lastline_message(buf);
  if (buf->firstLine == NULL) {
    /* FIXME: gettextize? */
    Strcat_charp(msg, "\tNo Line");
  }
  term_show_delayed_message();
  scr_standout();
  scr_message(msg->ptr, buf->cursorX + buf->rootX, buf->cursorY + buf->rootY);
  scr_standend();
  term_title(conv_to_system(buf->buffername));
  term_refresh();
  if (buf != save_current_buf) {
    saveBufferInfo();
    save_current_buf = buf;
  }
  if (mode == B_FORCE_REDRAW && (buf->check_url & CHK_URL)) {
    chkURLBuffer(buf);
    displayBuffer(buf, B_NORMAL);
  }
}

static void drawAnchorCursor0(struct Buffer *buf, AnchorList *al, int hseq,
                              int prevhseq, int tline, int eline, int active) {
  int i, j;
  Line *l;
  Anchor *an;

  l = buf->topLine;
  for (j = 0; j < al->nanchor; j++) {
    an = &al->anchors[j];
    if (an->start.line < tline)
      continue;
    if (an->start.line >= eline)
      return;
    for (;; l = l->next) {
      if (l == NULL)
        return;
      if (l->linenumber == an->start.line)
        break;
    }
    if (hseq >= 0 && an->hseq == hseq) {
      int start_pos = an->start.pos;
      int end_pos = an->end.pos;
      for (i = an->start.pos; i < an->end.pos; i++) {
        if (enable_inline_image && (l->propBuf[i] & PE_IMAGE)) {
          if (start_pos == i)
            start_pos = i + 1;
          else if (end_pos == an->end.pos)
            end_pos = i - 1;
        }
        if (l->propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM)) {
          if (active)
            l->propBuf[i] |= PE_ACTIVE;
          else
            l->propBuf[i] &= ~PE_ACTIVE;
        }
      }
      if (active && start_pos < end_pos)
        redrawLineRegion(buf, l, l->linenumber - tline + buf->rootY, start_pos,
                         end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
      if (active)
        redrawLineRegion(buf, l, l->linenumber - tline + buf->rootY,
                         an->start.pos, an->end.pos);
    }
  }
}

static void drawAnchorCursor(struct Buffer *buf) {
  Anchor *an;
  int hseq, prevhseq;
  int tline, eline;

  if (!buf->firstLine || !buf->hmarklist)
    return;
  if (!buf->href && !buf->formitem)
    return;

  an = retrieveCurrentAnchor(buf);
  if (!an)
    an = retrieveCurrentMap(buf);
  if (an)
    hseq = an->hseq;
  else
    hseq = -1;
  tline = buf->topLine->linenumber;
  eline = tline + buf->LINES;
  prevhseq = buf->hmarklist->prevhseq;

  if (buf->href) {
    drawAnchorCursor0(buf, buf->href, hseq, prevhseq, tline, eline, 1);
    drawAnchorCursor0(buf, buf->href, hseq, -1, tline, eline, 0);
  }
  if (buf->formitem) {
    drawAnchorCursor0(buf, buf->formitem, hseq, prevhseq, tline, eline, 1);
    drawAnchorCursor0(buf, buf->formitem, hseq, -1, tline, eline, 0);
  }
  buf->hmarklist->prevhseq = hseq;
}

static void redrawNLine(struct Buffer *buf, int n) {
  Line *l;
  int i;

  if (nTab > 1) {
    struct TabBuffer *t;
    int l;

    scr_move(0, 0);
    scr_clrtoeolx();
    for (t = FirstTab; t; t = t->nextTab) {
      scr_move(t->y, t->x1);
      if (t == CurrentTab)
        scr_bold();
      scr_addch('[');
      l = t->x2 - t->x1 - 1 - get_strwidth(t->currentBuffer->buffername);
      if (l < 0)
        l = 0;
      if (l / 2 > 0)
        scr_addnstr_sup(" ", l / 2);
      if (t == CurrentTab)
        EFFECT_ACTIVE_START;
      scr_addnstr(t->currentBuffer->buffername, t->x2 - t->x1 - l);
      if (t == CurrentTab)
        EFFECT_ACTIVE_END;
      if ((l + 1) / 2 > 0)
        scr_addnstr_sup(" ", (l + 1) / 2);
      scr_move(t->y, t->x2);
      scr_addch(']');
      if (t == CurrentTab)
        scr_boldend();
    }
    scr_move(LastTab->y + 1, 0);
    for (i = 0; i < COLS; i++)
      scr_addch('~');
  }
  for (i = 0, l = buf->topLine; i < buf->LINES; i++, l = l->next) {
    if (i >= buf->LINES - n || i < -n)
      l = redrawLine(buf, l, i + buf->rootY);
    if (l == NULL)
      break;
  }
  if (n > 0) {
    scr_move(i + buf->rootY, 0);
    scr_clrtobotx();
  }
}

static Line *redrawLine(struct Buffer *buf, Line *l, int i) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->currentColumn;
  char *p;
  Lineprop *pr;

  if (l == NULL) {
    return NULL;
  }
  scr_move(i, 0);
  if (showLineNum) {
    char tmp[16];
    if (!buf->rootX) {
      if (buf->lastLine->real_linenumber > 0)
        buf->rootX =
            (int)(log(buf->lastLine->real_linenumber + 0.1) / log(10)) + 2;
      if (buf->rootX < 5)
        buf->rootX = 5;
      if (buf->rootX > COLS)
        buf->rootX = COLS;
      buf->COLS = COLS - buf->rootX;
    }
    if (l->real_linenumber && !l->bpos)
      sprintf(tmp, "%*ld:", buf->rootX - 1, l->real_linenumber);
    else
      sprintf(tmp, "%*s ", buf->rootX - 1, "");
    scr_addstr(tmp);
  }
  scr_move(i, buf->rootX);
  if (l->width < 0)
    l->width = COLPOS(l, l->len);
  if (l->len == 0 || l->width - 1 < column) {
    scr_clrtoeolx();
    return l;
  }
  /* need_clrtoeol(); */
  pos = columnPos(l, column);
  p = &(l->lineBuf[pos]);
  pr = &(l->propBuf[pos]);
  rcol = COLPOS(l, pos);

  for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
    ncol = COLPOS(l, pos + j + delta);
    if (ncol - column > buf->COLS)
      break;
    if (rcol < column) {
      for (rcol = column; rcol < ncol; rcol++)
        addChar(' ', 0);
      continue;
    }
    if (p[j] == '\t') {
      for (; rcol < ncol; rcol++)
        addChar(' ', 0);
    } else {
      addChar(p[j], pr[j]);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = FALSE;
    scr_standend();
  }
  if (ulmode) {
    ulmode = FALSE;
    scr_underlineend();
  }
  if (bomode) {
    bomode = FALSE;
    scr_boldend();
  }
  if (emph_mode) {
    emph_mode = FALSE;
    scr_boldend();
  }

  if (anch_mode) {
    anch_mode = FALSE;
    EFFECT_ANCHOR_END;
  }
  if (imag_mode) {
    imag_mode = FALSE;
    EFFECT_IMAGE_END;
  }
  if (form_mode) {
    form_mode = FALSE;
    EFFECT_FORM_END;
  }
  if (visited_mode) {
    visited_mode = FALSE;
    EFFECT_VISITED_END;
  }
  if (active_mode) {
    active_mode = FALSE;
    EFFECT_ACTIVE_END;
  }
  if (mark_mode) {
    mark_mode = FALSE;
    EFFECT_MARK_END;
  }
  if (graph_mode) {
    graph_mode = FALSE;
    scr_graphend();
  }
  if (rcol - column < buf->COLS)
    scr_clrtoeolx();
  return l;
}

static int redrawLineRegion(struct Buffer *buf, Line *l, int i, int bpos,
                            int epos) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->currentColumn;
  char *p;
  Lineprop *pr;
  int bcol, ecol;

  if (l == NULL)
    return 0;
  pos = columnPos(l, column);
  p = &(l->lineBuf[pos]);
  pr = &(l->propBuf[pos]);
  rcol = COLPOS(l, pos);
  bcol = bpos - pos;
  ecol = epos - pos;

  for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
    ncol = COLPOS(l, pos + j + delta);
    if (ncol - column > buf->COLS)
      break;
    if (j >= bcol && j < ecol) {
      if (rcol < column) {
        scr_move(i, buf->rootX);
        for (rcol = column; rcol < ncol; rcol++)
          addChar(' ', 0);
        continue;
      }
      scr_move(i, rcol - column + buf->rootX);
      if (p[j] == '\t') {
        for (; rcol < ncol; rcol++)
          addChar(' ', 0);
      } else
        addChar(p[j], pr[j]);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = FALSE;
    scr_standend();
  }
  if (ulmode) {
    ulmode = FALSE;
    scr_underlineend();
  }
  if (bomode) {
    bomode = FALSE;
    scr_boldend();
  }
  if (emph_mode) {
    emph_mode = FALSE;
    scr_boldend();
  }

  if (anch_mode) {
    anch_mode = FALSE;
    EFFECT_ANCHOR_END;
  }
  if (imag_mode) {
    imag_mode = FALSE;
    EFFECT_IMAGE_END;
  }
  if (form_mode) {
    form_mode = FALSE;
    EFFECT_FORM_END;
  }
  if (visited_mode) {
    visited_mode = FALSE;
    EFFECT_VISITED_END;
  }
  if (active_mode) {
    active_mode = FALSE;
    EFFECT_ACTIVE_END;
  }
  if (mark_mode) {
    mark_mode = FALSE;
    EFFECT_MARK_END;
  }
  if (graph_mode) {
    graph_mode = FALSE;
    scr_graphend();
  }
  return rcol - column;
}

#define do_effect1(effect, modeflag, action_start, action_end)                 \
  if (m & effect) {                                                            \
    if (!modeflag) {                                                           \
      action_start;                                                            \
      modeflag = TRUE;                                                         \
    }                                                                          \
  }

#define do_effect2(effect, modeflag, action_start, action_end)                 \
  if (modeflag) {                                                              \
    action_end;                                                                \
    modeflag = FALSE;                                                          \
  }

static void do_effects(Lineprop m) {
  /* effect end */
  do_effect2(PE_UNDER, ulmode, underline(), scr_underlineend());
  do_effect2(PE_STAND, somode, standout(), scr_standend());
  do_effect2(PE_BOLD, bomode, bold(), scr_boldend());
  do_effect2(PE_EMPH, emph_mode, bold(), scr_boldend());
  do_effect2(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
  do_effect2(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
  do_effect2(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
  do_effect2(PE_VISITED, visited_mode, EFFECT_VISITED_START,
             EFFECT_VISITED_END);
  do_effect2(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
  do_effect2(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
  if (graph_mode) {
    scr_graphend();
    graph_mode = FALSE;
  }

  /* effect start */
  do_effect1(PE_UNDER, ulmode, scr_underline(), underlineend());
  do_effect1(PE_STAND, somode, scr_standout(), standend());
  do_effect1(PE_BOLD, bomode, scr_bold(), boldend());
  do_effect1(PE_EMPH, emph_mode, scr_bold(), boldend());
  do_effect1(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
  do_effect1(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
  do_effect1(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
  do_effect1(PE_VISITED, visited_mode, EFFECT_VISITED_START,
             EFFECT_VISITED_END);
  do_effect1(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
  do_effect1(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
}

void addChar(char c, Lineprop mode) {
  Lineprop m = CharEffect(mode);
  do_effects(m);
  if (mode & PC_SYMBOL) {
    char **symbol;
    c -= SYMBOL_BASE;
    if (term_graph_ok() && c < N_GRAPH_SYMBOL) {
      if (!graph_mode) {
        scr_graphstart();
        graph_mode = TRUE;
      }
      scr_addch(*graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
    } else {
      symbol = get_symbol();
      scr_addch(*symbol[(unsigned char)c % N_SYMBOL]);
    }
  } else if (mode & PC_CTRL) {
    switch (c) {
    case '\t':
      scr_addch(c);
      break;
    case '\n':
      scr_addch(' ');
      break;
    case '\r':
      break;
    case DEL_CODE:
      scr_addstr("^?");
      break;
    default:
      scr_addch('^');
      scr_addch(c + '@');
      break;
    }
  } else if (0x80 <= (unsigned char)c && (unsigned char)c <= NBSP_CODE)
    scr_addch(' ');
  else
    scr_addch(c);
}

/*
 * List of error messages
 */
struct Buffer *message_list_panel(void) {
  Str tmp = Strnew_size(LINES * COLS);

  /* FIXME: gettextize? */
  Strcat_charp(tmp,
               "<html><head><title>List of error messages</title></head><body>"
               "<h1>List of error messages</h1><table cellpadding=0>\n");

  Strcat_m_charp(tmp, term_message_to_html());

  Strcat_charp(tmp, "</table></body></html>");
  return loadHTMLString(tmp);
}

void cursorUp0(struct Buffer *buf, int n) {
  if (buf->cursorY > 0)
    cursorUpDown(buf, -1);
  else {
    buf->topLine = lineSkip(buf, buf->topLine, -n, FALSE);
    if (buf->currentLine->prev != NULL)
      buf->currentLine = buf->currentLine->prev;
    arrangeLine(buf);
  }
}

void cursorUp(struct Buffer *buf, int n) {
  Line *l = buf->currentLine;
  if (buf->firstLine == NULL)
    return;
  while (buf->currentLine->prev && buf->currentLine->bpos)
    cursorUp0(buf, n);
  if (buf->currentLine == buf->firstLine) {
    gotoLine(buf, l->linenumber);
    arrangeLine(buf);
    return;
  }
  cursorUp0(buf, n);
  while (buf->currentLine->prev && buf->currentLine->bpos &&
         buf->currentLine->bwidth >= buf->currentColumn + buf->visualpos)
    cursorUp0(buf, n);
}

void cursorDown0(struct Buffer *buf, int n) {
  if (buf->cursorY < buf->LINES - 1)
    cursorUpDown(buf, 1);
  else {
    buf->topLine = lineSkip(buf, buf->topLine, n, FALSE);
    if (buf->currentLine->next != NULL)
      buf->currentLine = buf->currentLine->next;
    arrangeLine(buf);
  }
}

void cursorDown(struct Buffer *buf, int n) {
  Line *l = buf->currentLine;
  if (buf->firstLine == NULL)
    return;
  while (buf->currentLine->next && buf->currentLine->next->bpos)
    cursorDown0(buf, n);
  if (buf->currentLine == buf->lastLine) {
    gotoLine(buf, l->linenumber);
    arrangeLine(buf);
    return;
  }
  cursorDown0(buf, n);
  while (buf->currentLine->next && buf->currentLine->next->bpos &&
         buf->currentLine->bwidth + buf->currentLine->width <
             buf->currentColumn + buf->visualpos)
    cursorDown0(buf, n);
}

void cursorUpDown(struct Buffer *buf, int n) {
  Line *cl = buf->currentLine;

  if (buf->firstLine == NULL)
    return;
  if ((buf->currentLine = currentLineSkip(buf, cl, n, FALSE)) == cl)
    return;
  arrangeLine(buf);
}

void cursorRight(struct Buffer *buf, int n) {
  int i, delta = 1, cpos, vpos2;
  Line *l = buf->currentLine;
  Lineprop *p;

  if (buf->firstLine == NULL)
    return;
  if (buf->pos == l->len && !(l->next && l->next->bpos))
    return;
  i = buf->pos;
  p = l->propBuf;
  if (i + delta < l->len) {
    buf->pos = i + delta;
  } else if (l->len == 0) {
    buf->pos = 0;
  } else if (l->next && l->next->bpos) {
    cursorDown0(buf, 1);
    buf->pos = 0;
    arrangeCursor(buf);
    return;
  } else {
    buf->pos = l->len - 1;
  }
  cpos = COLPOS(l, buf->pos);
  buf->visualpos = l->bwidth + cpos - buf->currentColumn;
  delta = 1;
  vpos2 = COLPOS(l, buf->pos + delta) - buf->currentColumn - 1;
  if (vpos2 >= buf->COLS && n) {
    columnSkip(buf, n + (vpos2 - buf->COLS) - (vpos2 - buf->COLS) % n);
    buf->visualpos = l->bwidth + cpos - buf->currentColumn;
  }
  buf->cursorX = buf->visualpos - l->bwidth;
}

void cursorLeft(struct Buffer *buf, int n) {
  int i, delta = 1, cpos;
  Line *l = buf->currentLine;
  Lineprop *p;

  if (buf->firstLine == NULL)
    return;
  i = buf->pos;
  p = l->propBuf;
  if (i >= delta)
    buf->pos = i - delta;
  else if (l->prev && l->bpos) {
    cursorUp0(buf, -1);
    buf->pos = buf->currentLine->len - 1;
    arrangeCursor(buf);
    return;
  } else
    buf->pos = 0;
  cpos = COLPOS(l, buf->pos);
  buf->visualpos = l->bwidth + cpos - buf->currentColumn;
  if (buf->visualpos - l->bwidth < 0 && n) {
    columnSkip(buf, -n + buf->visualpos - l->bwidth -
                        (buf->visualpos - l->bwidth) % n);
    buf->visualpos = l->bwidth + cpos - buf->currentColumn;
  }
  buf->cursorX = buf->visualpos - l->bwidth;
}

void cursorHome(struct Buffer *buf) {
  buf->visualpos = 0;
  buf->cursorX = buf->cursorY = 0;
}

/*
 * Arrange line,column and cursor position according to current line and
 * current position.
 */
void arrangeCursor(struct Buffer *buf) {
  int col, col2, pos;
  int delta = 1;
  if (buf == NULL || buf->currentLine == NULL)
    return;
  /* Arrange line */
  if (buf->currentLine->linenumber - buf->topLine->linenumber >= buf->LINES ||
      buf->currentLine->linenumber < buf->topLine->linenumber) {
    /*
     * buf->topLine = buf->currentLine;
     */
    buf->topLine = lineSkip(buf, buf->currentLine, 0, FALSE);
  }
  /* Arrange column */
  while (buf->pos < 0 && buf->currentLine->prev && buf->currentLine->bpos) {
    pos = buf->pos + buf->currentLine->prev->len;
    cursorUp0(buf, 1);
    buf->pos = pos;
  }
  while (buf->pos >= buf->currentLine->len && buf->currentLine->next &&
         buf->currentLine->next->bpos) {
    pos = buf->pos - buf->currentLine->len;
    cursorDown0(buf, 1);
    buf->pos = pos;
  }
  if (buf->currentLine->len == 0 || buf->pos < 0)
    buf->pos = 0;
  else if (buf->pos >= buf->currentLine->len)
    buf->pos = buf->currentLine->len - 1;
  col = COLPOS(buf->currentLine, buf->pos);
  col2 = COLPOS(buf->currentLine, buf->pos + delta);
  if (col < buf->currentColumn || col2 > buf->COLS + buf->currentColumn) {
    buf->currentColumn = 0;
    if (col2 > buf->COLS)
      columnSkip(buf, col);
  }
  /* Arrange cursor */
  buf->cursorY = buf->currentLine->linenumber - buf->topLine->linenumber;
  buf->visualpos = buf->currentLine->bwidth +
                   COLPOS(buf->currentLine, buf->pos) - buf->currentColumn;
  buf->cursorX = buf->visualpos - buf->currentLine->bwidth;
#ifdef DISPLAY_DEBUG
  fprintf(
      stderr,
      "arrangeCursor: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
      buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
      buf->currentLine->len);
#endif
}

void arrangeLine(struct Buffer *buf) {
  int i, cpos;

  if (buf->firstLine == NULL)
    return;
  buf->cursorY = buf->currentLine->linenumber - buf->topLine->linenumber;
  i = columnPos(buf->currentLine,
                buf->currentColumn + buf->visualpos - buf->currentLine->bwidth);
  cpos = COLPOS(buf->currentLine, i) - buf->currentColumn;
  if (cpos >= 0) {
    buf->cursorX = cpos;
    buf->pos = i;
  } else if (buf->currentLine->len > i) {
    buf->cursorX = 0;
    buf->pos = i + 1;
  } else {
    buf->cursorX = 0;
    buf->pos = 0;
  }
#ifdef DISPLAY_DEBUG
  fprintf(stderr,
          "arrangeLine: column=%d, cursorX=%d, visualpos=%d, pos=%d, len=%d\n",
          buf->currentColumn, buf->cursorX, buf->visualpos, buf->pos,
          buf->currentLine->len);
#endif
}

void cursorXY(struct Buffer *buf, int x, int y) {
  int oldX;

  cursorUpDown(buf, y - buf->cursorY);

  if (buf->cursorX > x) {
    while (buf->cursorX > x)
      cursorLeft(buf, buf->COLS / 2);
  } else if (buf->cursorX < x) {
    while (buf->cursorX < x) {
      oldX = buf->cursorX;

      cursorRight(buf, buf->COLS / 2);

      if (oldX == buf->cursorX)
        break;
    }
    if (buf->cursorX > x)
      cursorLeft(buf, buf->COLS / 2);
  }
}

void restorePosition(struct Buffer *buf, struct Buffer *orig) {
  buf->topLine = lineSkip(buf, buf->firstLine, TOP_LINENUMBER(orig) - 1, FALSE);
  gotoLine(buf, CUR_LINENUMBER(orig));
  buf->pos = orig->pos;
  if (buf->currentLine && orig->currentLine)
    buf->pos += orig->currentLine->bpos - buf->currentLine->bpos;
  buf->currentColumn = orig->currentColumn;
  arrangeCursor(buf);
}
