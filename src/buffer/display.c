#include "buffer/display.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/tabbuffer.h"
#include "html/html_readbuffer.h"
#include "html/html_renderer.h"
#include "html/map.h"
#include "html/table.h"
#include "input/url_stream.h"
#include "term/scr.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "text/ctrlcode.h"
#include "text/symbol.h"
#include "text/utf8.h"

int nTab = 0;
int enable_inline_image;
bool displayLink = false;
bool displayLineInfo = false;

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
static struct Line *cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;

static struct Buffer *save_current_buf = NULL;

static void drawAnchorCursor(struct Buffer *buf);
#define redrawBuffer(buf) redrawNLine(buf, LASTLINE)
static void redrawNLine(struct Buffer *buf, int n);
static struct Line *redrawLine(struct Buffer *buf, struct Line *l, int i);
static int redrawLineRegion(struct Buffer *buf, struct Line *l, int i, int bpos,
                            int epos);
static void do_effects(Lineprop m);

static Str make_lastline_link(struct Url *base, const char *title,
                              const char *url) {
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
    l -= utf8str_width((const uint8_t *)s);
    if (l <= 0)
      return s;
  }
  if (!url)
    return s;
  parseURL2(url, &pu, base);
  u = parsedURL2Str(&pu);
  if (DecodeURL)
    u = Strnew_charp(url_decode0(u->ptr));
  if (l <= 4 || l >= utf8str_width((const uint8_t *)u->ptr)) {
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
  i = utf8str_width((const uint8_t *)u->ptr) -
      (COLS - 1 - utf8str_width((const uint8_t *)s->ptr));
  Strcat_charp(s, &u->ptr[i]);
  return s;
}

static Str make_lastline_message(struct Buffer *buf) {
  Str s = NULL;
  int sl = 0;
  if (displayLink) {
    {
      struct Anchor *a = retrieveCurrentAnchor(buf->document);
      const char *p = NULL;
      if (a && a->title && *a->title)
        p = a->title;
      else {
        struct Anchor *a_img = retrieveCurrentImg(buf->document);
        if (a_img && a_img->title && *a_img->title)
          p = a_img->title;
      }
      if (p || a)
        s = make_lastline_link(baseURL(buf), p, a ? a->url : NULL);
    }
    if (s) {
      sl = utf8str_width((const uint8_t *)s->ptr);
      if (sl >= COLS - 3)
        return s;
    }
  }

  auto msg = Strnew();
  if (displayLineInfo && buf->document->currentLine != NULL &&
      buf->document->lastLine != NULL) {
    int cl = buf->document->currentLine->real_linenumber;
    int ll = buf->document->lastLine->real_linenumber;
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
    if (utf8str_width((const uint8_t *)msg->ptr) > l) {
      Strtruncate(msg, l);
    }
    Strcat_charp(msg, "> ");
    Strcat(msg, s);
  } else {
    Strcat_charp(msg, ">");
  }
  return msg;
}

void displayBuffer(struct Buffer *buf, enum DisplayMode mode) {
  Str msg;
  int ny = 0;

  if (!buf)
    return;
  if (buf->document->topLine == NULL &&
      readBufferCache(buf->document) == 0) { /* clear_buffer */
    mode = B_FORCE_REDRAW;
  }

  if (buf->document->width == 0)
    buf->document->width = INIT_BUFFER_WIDTH;
  if (buf->document->height == 0)
    buf->document->height = LASTLINE + 1;
  if ((buf->document->width != INIT_BUFFER_WIDTH &&
       (is_html_type(buf->type) || FoldLine)) ||
      buf->need_reshape) {
    buf->need_reshape = true;
    reshapeBuffer(buf);
  }
  if (showLineNum) {
    if (buf->document->lastLine && buf->document->lastLine->real_linenumber > 0)
      buf->document->rootX =
          (int)(log(buf->document->lastLine->real_linenumber + 0.1) / log(10)) +
          2;
    if (buf->document->rootX < 5)
      buf->document->rootX = 5;
    if (buf->document->rootX > COLS)
      buf->document->rootX = COLS;
  } else
    buf->document->rootX = 0;
  buf->document->COLS = COLS - buf->document->rootX;
  if (nTab > 1) {
    if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
      calcTabPos();
    ny = LastTab->y + 2;
    if (ny > LASTLINE)
      ny = LASTLINE;
  }
  if (buf->document->rootY != ny || buf->document->LINES != LASTLINE - ny) {
    buf->document->rootY = ny;
    buf->document->LINES = LASTLINE - ny;
    arrangeCursor(buf->document);
    mode = B_REDRAW_IMAGE;
  }
  if (mode == B_FORCE_REDRAW || mode == B_SCROLL || mode == B_REDRAW_IMAGE ||
      cline != buf->document->topLine ||
      ccolumn != buf->document->currentColumn) {
    {
      redrawBuffer(buf);
    }
    cline = buf->document->topLine;
    ccolumn = buf->document->currentColumn;
  }
  if (buf->document->topLine == NULL)
    buf->document->topLine = buf->document->firstLine;

  drawAnchorCursor(buf);

  msg = make_lastline_message(buf);
  if (buf->document->firstLine == NULL) {
    /* FIXME: gettextize? */
    Strcat_charp(msg, "\tNo Line");
  }
  term_show_delayed_message();
  scr_standout();
  scr_message(msg->ptr, buf->document->cursorX + buf->document->rootX,
              buf->document->cursorY + buf->document->rootY);
  scr_standend();
  term_title(buf->buffername);
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

static void drawAnchorCursor0(struct Buffer *buf, struct AnchorList *al,
                              int hseq, int prevhseq, int tline, int eline,
                              int active) {
  int i;

  auto l = buf->document->topLine;
  for (int j = 0; j < al->nanchor; j++) {
    auto an = &al->anchors[j];
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
      for (int i = an->start.pos; i < an->end.pos; i++) {
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
        redrawLineRegion(buf, l, l->linenumber - tline + buf->document->rootY,
                         start_pos, end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
      if (active)
        redrawLineRegion(buf, l, l->linenumber - tline + buf->document->rootY,
                         an->start.pos, an->end.pos);
    }
  }
}

static void drawAnchorCursor(struct Buffer *buf) {
  struct Anchor *an;
  int hseq, prevhseq;
  int tline, eline;

  if (!buf->document->firstLine || !buf->document->hmarklist)
    return;
  if (!buf->document->href && !buf->document->formitem)
    return;

  an = retrieveCurrentAnchor(buf->document);
  if (!an)
    an = retrieveCurrentMap(buf);
  if (an)
    hseq = an->hseq;
  else
    hseq = -1;
  tline = buf->document->topLine->linenumber;
  eline = tline + buf->document->LINES;
  prevhseq = buf->document->hmarklist->prevhseq;

  if (buf->document->href) {
    drawAnchorCursor0(buf, buf->document->href, hseq, prevhseq, tline, eline,
                      1);
    drawAnchorCursor0(buf, buf->document->href, hseq, -1, tline, eline, 0);
  }
  if (buf->document->formitem) {
    drawAnchorCursor0(buf, buf->document->formitem, hseq, prevhseq, tline,
                      eline, 1);
    drawAnchorCursor0(buf, buf->document->formitem, hseq, -1, tline, eline, 0);
  }
  buf->document->hmarklist->prevhseq = hseq;
}

static void redrawNLine(struct Buffer *buf, int n) {
  struct Line *l;
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
      l = t->x2 - t->x1 - 1 -
          utf8str_width((const uint8_t *)t->currentBuffer->buffername);
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
  for (i = 0, l = buf->document->topLine; i < buf->document->LINES;
       i++, l = l->next) {
    if (i >= buf->document->LINES - n || i < -n)
      l = redrawLine(buf, l, i + buf->document->rootY);
    if (l == NULL)
      break;
  }
  if (n > 0) {
    scr_move(i + buf->document->rootY, 0);
    scr_clrtobotx();
  }
}

void addMChar(const uint8_t *p, Lineprop mode, size_t len) {
  Lineprop m = CharEffect(mode);
  char c = *p;

  if (mode & PC_WCHAR2)
    return;
  do_effects(m);
  if (mode & PC_SYMBOL) {
    char **symbol;
    int w = (mode & PC_KANJI) ? 2 : 1;

    // c = ((char)wtf_get_code((wc_uchar *)p) & 0x7f) - SYMBOL_BASE;
    if (term_graph_ok() && c < N_GRAPH_SYMBOL) {
      if (!graph_mode) {
        scr_graphstart();
        graph_mode = true;
      }
      // if (w == 2 && WcOption.use_wide)
      //   addstr(graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
      // else
      scr_addch(*graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
    } else {
      symbol = get_symbol();
      scr_addstr(symbol[(unsigned char)c % N_SYMBOL]);
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
  }
  // else if (mode & PC_UNKNOWN) {
  //   char buf[5];
  //   sprintf(buf, "[%.2X]", (unsigned char)wtf_get_code((wc_uchar *)p) |
  //   0x80); scr_addstr(buf);
  // }
  else {
    scr_addutf8(p);
  }
}

void addChar(char c, Lineprop mode) { addMChar((const uint8_t *)&c, mode, 1); }

static struct Line *redrawLine(struct Buffer *buf, struct Line *l, int i) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->document->currentColumn;
  char *p;
  Lineprop *pr;

  if (l == NULL) {
    return NULL;
  }
  scr_move(i, 0);
  if (showLineNum) {
    char tmp[16];
    if (!buf->document->rootX) {
      if (buf->document->lastLine->real_linenumber > 0)
        buf->document->rootX =
            (int)(log(buf->document->lastLine->real_linenumber + 0.1) /
                  log(10)) +
            2;
      if (buf->document->rootX < 5)
        buf->document->rootX = 5;
      if (buf->document->rootX > COLS)
        buf->document->rootX = COLS;
      buf->document->COLS = COLS - buf->document->rootX;
    }
    if (l->real_linenumber && !l->bpos)
      sprintf(tmp, "%*ld:", buf->document->rootX - 1, l->real_linenumber);
    else
      sprintf(tmp, "%*s ", buf->document->rootX - 1, "");
    scr_addstr(tmp);
  }
  scr_move(i, buf->document->rootX);
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

  for (j = 0; rcol - column < buf->document->COLS && pos + j < l->len;
       j += delta) {
    delta = utf8sequence_len((const uint8_t *)&p[j]);
    ncol = COLPOS(l, pos + j + delta);
    if (ncol - column > buf->document->COLS)
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
      addMChar((const uint8_t *)&p[j], pr[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    scr_standend();
  }
  if (ulmode) {
    ulmode = false;
    scr_underlineend();
  }
  if (bomode) {
    bomode = false;
    scr_boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    scr_boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    EFFECT_ANCHOR_END;
  }
  if (imag_mode) {
    imag_mode = false;
    EFFECT_IMAGE_END;
  }
  if (form_mode) {
    form_mode = false;
    EFFECT_FORM_END;
  }
  if (visited_mode) {
    visited_mode = false;
    EFFECT_VISITED_END;
  }
  if (active_mode) {
    active_mode = false;
    EFFECT_ACTIVE_END;
  }
  if (mark_mode) {
    mark_mode = false;
    EFFECT_MARK_END;
  }
  if (graph_mode) {
    graph_mode = false;
    scr_graphend();
  }
  if (rcol - column < buf->document->COLS)
    scr_clrtoeolx();
  return l;
}

static int redrawLineRegion(struct Buffer *buf, struct Line *l, int i, int bpos,
                            int epos) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->document->currentColumn;
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

  for (j = 0; rcol - column < buf->document->COLS && pos + j < l->len;
       j += delta) {
    ncol = COLPOS(l, pos + j + delta);
    if (ncol - column > buf->document->COLS)
      break;
    if (j >= bcol && j < ecol) {
      if (rcol < column) {
        scr_move(i, buf->document->rootX);
        for (rcol = column; rcol < ncol; rcol++)
          addChar(' ', 0);
        continue;
      }
      scr_move(i, rcol - column + buf->document->rootX);
      if (p[j] == '\t') {
        for (; rcol < ncol; rcol++)
          addChar(' ', 0);
      } else
        addChar(p[j], pr[j]);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    scr_standend();
  }
  if (ulmode) {
    ulmode = false;
    scr_underlineend();
  }
  if (bomode) {
    bomode = false;
    scr_boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    scr_boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    EFFECT_ANCHOR_END;
  }
  if (imag_mode) {
    imag_mode = false;
    EFFECT_IMAGE_END;
  }
  if (form_mode) {
    form_mode = false;
    EFFECT_FORM_END;
  }
  if (visited_mode) {
    visited_mode = false;
    EFFECT_VISITED_END;
  }
  if (active_mode) {
    active_mode = false;
    EFFECT_ACTIVE_END;
  }
  if (mark_mode) {
    mark_mode = false;
    EFFECT_MARK_END;
  }
  if (graph_mode) {
    graph_mode = false;
    scr_graphend();
  }
  return rcol - column;
}

#define do_effect1(effect, modeflag, action_start, action_end)                 \
  if (m & effect) {                                                            \
    if (!modeflag) {                                                           \
      action_start;                                                            \
      modeflag = true;                                                         \
    }                                                                          \
  }

#define do_effect2(effect, modeflag, action_start, action_end)                 \
  if (modeflag) {                                                              \
    action_end;                                                                \
    modeflag = false;                                                          \
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
    graph_mode = false;
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

/*
 * List of error messages
 */
struct Document *message_list_panel(void) {
  Str tmp = Strnew_size(LINES * COLS);

  /* FIXME: gettextize? */
  Strcat_charp(tmp,
               "<html><head><title>List of error messages</title></head><body>"
               "<h1>List of error messages</h1><table cellpadding=0>\n");

  Strcat_m_charp(tmp, term_message_to_html());

  Strcat_charp(tmp, "</table></body></html>");
  struct Url url;
  return loadHTML(tmp->ptr, url, nullptr);
}

void displayInvalidate() { displayBuffer(Currentbuf, B_NORMAL); }
