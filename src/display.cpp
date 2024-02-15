#include "display.h"
#include "app.h"
#include "line_layout.h"
#include "url_stream.h"
#include "tabbuffer.h"
#include "http_session.h"
#include "http_response.h"
#include "symbol.h"
#include "message.h"
#include "w3m.h"
#include "readbuffer.h"
#include "screen.h"
#include "rc.h"
#include "utf8.h"
#include "terms.h"
#include "buffer.h"
#include "textlist.h"
#include "ctrlcode.h"
#include "anchor.h"
#include "signal_util.h"
#include "proto.h"
#include <math.h>

bool displayLink = false;
bool showLineNum = false;
bool FoldLine = false;
int _INIT_BUFFER_WIDTH() { return (COLS - (showLineNum ? 6 : 1)); }
int INIT_BUFFER_WIDTH() {
  return ((_INIT_BUFFER_WIDTH() > 0) ? _INIT_BUFFER_WIDTH() : 0);
}
int FOLD_BUFFER_WIDTH() { return (FoldLine ? (INIT_BUFFER_WIDTH() + 1) : -1); }

/* *INDENT-OFF* */

#define EFFECT_ANCHOR_START underline()
#define EFFECT_ANCHOR_END underlineend()
#define EFFECT_IMAGE_START standout()
#define EFFECT_IMAGE_END standend()
#define EFFECT_FORM_START standout()
#define EFFECT_FORM_END standend()
#define EFFECT_ACTIVE_START bold()
#define EFFECT_ACTIVE_END boldend()
#define EFFECT_VISITED_START /**/
#define EFFECT_VISITED_END   /**/
#define EFFECT_MARK_START standout()
#define EFFECT_MARK_END standend()
/* *INDENT-ON* */

void fmTerm(void) {
  if (fmInitialized) {
    move(LASTLINE, 0);
    clrtoeolx();
    refresh(term_io());
    reset_tty();
    fmInitialized = false;
  }
}

/*
 * Initialize routine.
 */
void fmInit(void) {
  if (!fmInitialized) {
    initscr();
    term_raw();
    term_noecho();
  }
  fmInitialized = true;
}

/*
 * Display some lines.
 */
static Line *cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;

static std::shared_ptr<Buffer> save_current_buf;

static void drawAnchorCursor(const std::shared_ptr<Buffer> &buf);
#define redrawBuffer(buf) redrawNLine(buf, LASTLINE)
static void redrawNLine(const std::shared_ptr<Buffer> &buf, int n);
static Line *redrawLine(LineLayout *buf, Line *l, int i);
static int redrawLineRegion(const std::shared_ptr<Buffer> &buf, Line *l, int i,
                            int bpos, int epos);
static void do_effects(Lineprop m);

static Str *make_lastline_link(const std::shared_ptr<Buffer> &buf,
                               const char *title, const char *url) {
  Str *s = NULL;
  Str *u;
  Url pu;
  const char *p;
  int l = COLS - 1, i;

  if (title && *title) {
    s = Strnew_m_charp("[", title, "]", NULL);
    for (p = s->ptr; *p; p++) {
      if (IS_CNTRL(*p) || IS_SPACE(*p))
        *(char *)p = ' ';
    }
    if (url)
      Strcat_charp(s, " ");
    l -= get_Str_strwidth(s);
    if (l <= 0)
      return s;
  }
  if (!url)
    return s;
  pu = urlParse(url, buf->res->getBaseURL());
  u = Strnew(pu.to_Str());
  if (DecodeURL)
    u = Strnew_charp(url_decode0(u->ptr));
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

static Str *make_lastline_message(const std::shared_ptr<Buffer> &buf) {
  Str *msg;
  Str *s = NULL;
  int sl = 0;

  if (displayLink) {
    {
      Anchor *a = buf->layout.retrieveCurrentAnchor();
      const char *p = NULL;
      if (a && a->title && *a->title)
        p = a->title;
      else {
        auto a_img = buf->layout.retrieveCurrentImg();
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
  if (displayLineInfo && buf->layout.currentLine != NULL &&
      buf->layout.lastLine != NULL) {
    int cl = buf->layout.currentLine->real_linenumber;
    int ll = buf->layout.lastLine->real_linenumber;
    int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
    Strcat(msg, Sprintf("%d/%d (%d%%)", cl, ll, r));
  } else {
    Strcat_charp(msg, "Viewing");
  }
  if (buf->res->ssl_certificate) {
    Strcat_charp(msg, "[SSL]");
  }
  Strcat_charp(msg, " <");
  Strcat(msg, buf->layout.title);

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

static bool need_resize_screen = false;

void setResize() { need_resize_screen = true; }

void displayBuffer() {
  if (need_resize_screen) {
    need_resize_screen = false;
    setlinescols();
    setupscreen(term_entry());
  }

  auto buf = CurrentTab->currentBuffer();
  if (!buf) {
    return;
  }

  if (buf->layout.width == 0)
    buf->layout.width = INIT_BUFFER_WIDTH();
  if (buf->layout.height == 0)
    buf->layout.height = LASTLINE + 1;
  if ((buf->layout.width != INIT_BUFFER_WIDTH() &&
       (buf->res->is_html_type() || FoldLine)) ||
      buf->layout.need_reshape) {
    buf->layout.need_reshape = true;
    reshapeBuffer(buf);
  }

  // Str *msg;
  int ny = 0;
  auto &layout = buf->layout;
  if (showLineNum) {
    if (layout.lastLine && layout.lastLine->real_linenumber > 0)
      layout.rootX =
          (int)(log(layout.lastLine->real_linenumber + 0.1) / log(10)) + 2;
    if (layout.rootX < 5)
      layout.rootX = 5;
    if (layout.rootX > COLS)
      layout.rootX = COLS;
  } else
    layout.rootX = 0;
  layout.COLS = COLS - layout.rootX;
  if (App::instance().nTab() > 1) {
    ny = App::instance().calcTabPos() + 2;
    if (ny > LASTLINE)
      ny = LASTLINE;
  }
  if (layout.rootY != ny || layout.LINES != LASTLINE - ny) {
    layout.rootY = ny;
    layout.LINES = LASTLINE - ny;
    buf->layout.arrangeCursor();
  }
  if (cline != buf->layout.topLine || ccolumn != buf->layout.currentColumn) {
    {
      redrawBuffer(buf);
    }
    cline = buf->layout.topLine;
    ccolumn = buf->layout.currentColumn;
  }
  if (buf->layout.topLine == NULL)
    buf->layout.topLine = buf->layout.firstLine;

  drawAnchorCursor(buf);

  auto msg = make_lastline_message(buf);
  if (buf->layout.firstLine == NULL) {
    Strcat_charp(msg, "\tNo Line");
  }

  refresh_message();

  standout();
  message(msg->ptr, buf->layout.AbsCursorX(), buf->layout.AbsCursorY());
  standend();
  term_title(buf->layout.title.c_str());
  refresh(term_io());
  if (buf != save_current_buf) {
    CurrentTab->currentBuffer()->saveBufferInfo();
    save_current_buf = buf;
  }
  if (buf->check_url) {
    chkURLBuffer(buf);
    displayBuffer();
  }
}

static void drawAnchorCursor0(const std::shared_ptr<Buffer> &buf,
                              AnchorList *al, int hseq, int prevhseq, int tline,
                              int eline, int active) {
  auto l = buf->layout.topLine;
  for (size_t j = 0; j < al->size(); j++) {
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
        if (l->propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM)) {
          if (active)
            l->propBuf[i] |= PE_ACTIVE;
          else
            l->propBuf[i] &= ~PE_ACTIVE;
        }
      }
      if (active && start_pos < end_pos)
        redrawLineRegion(buf, l, l->linenumber - tline + buf->layout.rootY,
                         start_pos, end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
      if (active)
        redrawLineRegion(buf, l, l->linenumber - tline + buf->layout.rootY,
                         an->start.pos, an->end.pos);
    }
  }
}

static void drawAnchorCursor(const std::shared_ptr<Buffer> &buf) {
  int hseq, prevhseq;
  int tline, eline;

  if (!buf->layout.firstLine || !buf->layout.hmarklist())
    return;
  if (!buf->layout.href() && !buf->layout.formitem())
    return;

  auto an = buf->layout.retrieveCurrentAnchor();
  if (an) {
    hseq = an->hseq;
  } else {
    hseq = -1;
  }
  tline = buf->layout.topLine->linenumber;
  eline = tline + buf->layout.LINES;
  prevhseq = buf->layout.hmarklist()->prevhseq;

  if (buf->layout.href()) {
    drawAnchorCursor0(buf, buf->layout.href().get(), hseq, prevhseq, tline,
                      eline, 1);
    drawAnchorCursor0(buf, buf->layout.href().get(), hseq, -1, tline, eline, 0);
  }
  if (buf->layout.formitem()) {
    drawAnchorCursor0(buf, buf->layout.formitem().get(), hseq, prevhseq, tline,
                      eline, 1);
    drawAnchorCursor0(buf, buf->layout.formitem().get(), hseq, -1, tline, eline,
                      0);
  }
  buf->layout.hmarklist()->prevhseq = hseq;
}

static void redrawNLine(const std::shared_ptr<Buffer> &buf, int n) {
  App::instance().drawTabs();
  {
    Line *l;
    int i;
    for (i = 0, l = buf->layout.topLine; i < buf->layout.LINES;
         i++, l = l->next) {
      if (i >= buf->layout.LINES - n || i < -n)
        l = redrawLine(&buf->layout, l, i + buf->layout.rootY);
      if (l == NULL)
        break;
    }
    if (n > 0) {
      move(i + buf->layout.rootY, 0);
      clrtobotx();
    }
  }
}

static Line *redrawLine(LineLayout *buf, Line *l, int i) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->currentColumn;
  char *p;
  Lineprop *pr;

  if (l == NULL) {
    return NULL;
  }
  move(i, 0);
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
      snprintf(tmp, sizeof(tmp), "%*ld:", buf->rootX - 1, l->real_linenumber);
    else
      snprintf(tmp, sizeof(tmp), "%*s ", buf->rootX - 1, "");
    addstr(tmp);
  }
  move(i, buf->rootX);
  if (l->len == 0 || l->width() - 1 < column) {
    clrtoeolx();
    return l;
  }
  /* need_clrtoeol(); */
  pos = l->columnPos(column);
  p = &(l->lineBuf[pos]);
  pr = &(l->propBuf[pos]);
  rcol = l->bytePosToColumn(pos);

  for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
    delta = get_mclen(&p[j]);
    ncol = l->bytePosToColumn(pos + j + delta);
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
      addMChar(&p[j], pr[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    standend();
  }
  if (ulmode) {
    ulmode = false;
    underlineend();
  }
  if (bomode) {
    bomode = false;
    boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    boldend();
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
    graphend();
  }
  if (rcol - column < buf->COLS)
    clrtoeolx();
  return l;
}

static int redrawLineRegion(const std::shared_ptr<Buffer> &buf, Line *l, int i,
                            int bpos, int epos) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->layout.currentColumn;
  char *p;
  Lineprop *pr;
  int bcol, ecol;

  if (l == NULL)
    return 0;
  pos = l->columnPos(column);
  p = &(l->lineBuf[pos]);
  pr = &(l->propBuf[pos]);
  rcol = l->bytePosToColumn(pos);
  bcol = bpos - pos;
  ecol = epos - pos;

  for (j = 0; rcol - column < buf->layout.COLS && pos + j < l->len;
       j += delta) {
    delta = get_mclen(&p[j]);
    ncol = l->bytePosToColumn(pos + j + delta);
    if (ncol - column > buf->layout.COLS)
      break;
    if (j >= bcol && j < ecol) {
      if (rcol < column) {
        move(i, buf->layout.rootX);
        for (rcol = column; rcol < ncol; rcol++)
          addChar(' ', 0);
        continue;
      }
      move(i, rcol - column + buf->layout.rootX);
      if (p[j] == '\t') {
        for (; rcol < ncol; rcol++)
          addChar(' ', 0);
      } else
        addMChar(&p[j], pr[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    standend();
  }
  if (ulmode) {
    ulmode = false;
    underlineend();
  }
  if (bomode) {
    bomode = false;
    boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    boldend();
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
    graphend();
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
  do_effect2(PE_UNDER, ulmode, underline(), underlineend());
  do_effect2(PE_STAND, somode, standout(), standend());
  do_effect2(PE_BOLD, bomode, bold(), boldend());
  do_effect2(PE_EMPH, emph_mode, bold(), boldend());
  do_effect2(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
  do_effect2(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
  do_effect2(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
  do_effect2(PE_VISITED, visited_mode, EFFECT_VISITED_START,
             EFFECT_VISITED_END);
  do_effect2(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
  do_effect2(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
  if (graph_mode) {
    graphend();
    graph_mode = false;
  }

  /* effect start */
  do_effect1(PE_UNDER, ulmode, underline(), underlineend());
  do_effect1(PE_STAND, somode, standout(), standend());
  do_effect1(PE_BOLD, bomode, bold(), boldend());
  do_effect1(PE_EMPH, emph_mode, bold(), boldend());
  do_effect1(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
  do_effect1(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
  do_effect1(PE_FORM, form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
  do_effect1(PE_VISITED, visited_mode, EFFECT_VISITED_START,
             EFFECT_VISITED_END);
  do_effect1(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
  do_effect1(PE_MARK, mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
}

void addMChar(char *p, Lineprop mode, size_t len) {
  Lineprop m = CharEffect(mode);
  char c = *p;

  if (mode & PC_WCHAR2)
    return;
  do_effects(m);
  if (mode & PC_SYMBOL) {
    const char **symbol;
    int w = (mode & PC_KANJI) ? 2 : 1;

    // c = ((char)wtf_get_code((wc_uchar *)p) & 0x7f) - SYMBOL_BASE;
    if (graph_ok() && c < N_GRAPH_SYMBOL) {
      if (!graph_mode) {
        graphstart();
        graph_mode = true;
      }
      if (w == 2)
        addstr(graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
      else
        addch(*graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
    } else {
      symbol = get_symbol();
      addstr(symbol[(unsigned char)c % N_SYMBOL]);
    }
  } else if (mode & PC_CTRL) {
    switch (c) {
    case '\t':
      addch(c);
      break;
    case '\n':
      addch(' ');
      break;
    case '\r':
      break;
    case DEL_CODE:
      addstr("^?");
      break;
    default:
      addch('^');
      addch(c + '@');
      break;
    }
  } else if (mode & PC_UNKNOWN) {
    // char buf[5];
    // sprintf(buf, "[%.2X]", (unsigned char)wtf_get_code((wc_uchar *)p) |
    // 0x80); addstr(buf);
    addstr("[xx]");
  } else {
    addmch(Utf8::from((const char8_t *)p, len));
  }
}

void addChar(char c, Lineprop mode) { addMChar(&c, mode, 1); }
