#include "display.h"
#include "app.h"
#include "line_layout.h"
#include "url_stream.h"
#include "tabbuffer.h"
#include "http_session.h"
#include "http_response.h"
#include "symbol.h"
#include "w3m.h"
#include "html/readbuffer.h"
#include "screen.h"
#include "rc.h"
#include "utf8.h"
#include "buffer.h"
#include "textlist.h"
#include "ctrlcode.h"
#include "html/anchor.h"
#include "proto.h"
#include <math.h>
#include <iostream>

bool displayLink = false;

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

/*
 * Display some lines.
 */
static Line *cline = NULL;
static int ccolumn = -1;

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0, graph_mode = 0;

static std::shared_ptr<Buffer> save_current_buf;

static Str *make_lastline_link(TermScreen *screen,
                               const std::shared_ptr<Buffer> &buf,
                               const char *title, const char *url) {
  Str *s = NULL;
  Str *u;
  Url pu;
  const char *p;
  int l = screen->COLS() - 1, i;

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
    s = Strnew_size(screen->COLS());
  i = (l - 2) / 2;
  Strcat_charp_n(s, u->ptr, i);
  Strcat_charp(s, "..");
  i = get_Str_strwidth(u) - (screen->COLS() - 1 - get_Str_strwidth(s));
  Strcat_charp(s, &u->ptr[i]);
  return s;
}

static Str *make_lastline_message(TermScreen *screen,
                                  const std::shared_ptr<Buffer> &buf) {
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
        s = make_lastline_link(screen, buf, p, a ? a->url : NULL);
    }
    if (s) {
      sl = get_Str_strwidth(s);
      if (sl >= screen->COLS() - 3)
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
    int l = screen->COLS() - 3 - sl;
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

static Line *redrawLine(TermScreen *screen, LineLayout *buf, Line *l, int i) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->currentColumn;
  char *p;

  if (l == NULL) {
    return NULL;
  }
  RowCol pixel{.row = i, .col = buf->rootX};
  if (l->len == 0 || l->width() - 1 < column) {
    screen->clrtoeolx(pixel);
    return l;
  }
  /* need_clrtoeol(); */
  pos = l->columnPos(column);
  p = &(l->lineBuf[pos]);
  // pr = &(l->propBuf[pos]);
  rcol = l->bytePosToColumn(pos);

  for (j = 0; rcol - column < buf->COLS && pos + j < l->len; j += delta) {
    delta = get_mclen(&p[j]);
    ncol = l->bytePosToColumn(pos + j + delta);
    if (ncol - column > buf->COLS)
      break;
    if (rcol < column) {
      for (rcol = column; rcol < ncol; rcol++)
        pixel = screen->addch(pixel, ' ');
      continue;
    }
    if (p[j] == '\t') {
      for (; rcol < ncol; rcol++)
        pixel = screen->addch(pixel, ' ');
    } else {
      pixel = screen->addnstr(pixel, &p[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    screen->standend();
  }
  if (ulmode) {
    ulmode = false;
    screen->underlineend();
  }
  if (bomode) {
    bomode = false;
    screen->boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    screen->boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    screen->EFFECT_ANCHOR_END;
  }
  if (imag_mode) {
    imag_mode = false;
    screen->EFFECT_IMAGE_END;
  }
  if (form_mode) {
    form_mode = false;
    screen->EFFECT_FORM_END;
  }
  if (visited_mode) {
    visited_mode = false;
    EFFECT_VISITED_END;
  }
  if (active_mode) {
    active_mode = false;
    screen->EFFECT_ACTIVE_END;
  }
  if (mark_mode) {
    mark_mode = false;
    screen->EFFECT_MARK_END;
  }
  if (graph_mode) {
    graph_mode = false;
    screen->graphend();
  }
  if (rcol - column < buf->COLS)
    screen->clrtoeolx(pixel);
  return l;
}

static void redrawNLine(TermScreen *screen, const std::shared_ptr<Buffer> &buf,
                        int n) {
  {
    Line *l;
    int i;
    for (i = 0, l = buf->layout.topLine; i < buf->layout.LINES;
         i++, l = l->next) {
      if (i >= buf->layout.LINES - n || i < -n)
        l = redrawLine(screen, &buf->layout, l, i + buf->layout.rootY);
      if (l == NULL)
        break;
    }
    if (n > 0) {
      screen->clrtobotx({.row = i + buf->layout.rootY, .col = 0});
    }
  }
}

static int redrawLineRegion(TermScreen *screen,
                            const std::shared_ptr<Buffer> &buf, Line *l, int i,
                            int bpos, int epos) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->layout.currentColumn;
  char *p;
  // Lineprop *pr;
  int bcol, ecol;

  if (l == NULL)
    return 0;
  pos = l->columnPos(column);
  p = &(l->lineBuf[pos]);
  // pr = &(l->propBuf[pos]);
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
        RowCol pixel{.row = i, .col = buf->layout.rootX};
        for (rcol = column; rcol < ncol; rcol++)
          pixel = screen->addch(pixel, ' ');
        continue;
      }
      RowCol pixel{.row = i, .col = rcol - column + buf->layout.rootX};
      if (p[j] == '\t') {
        for (; rcol < ncol; rcol++)
          pixel = screen->addch(pixel, ' ');
      } else
        pixel = screen->addnstr(pixel, &p[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    screen->standend();
  }
  if (ulmode) {
    ulmode = false;
    screen->underlineend();
  }
  if (bomode) {
    bomode = false;
    screen->boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    screen->boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    screen->EFFECT_ANCHOR_END;
  }
  if (imag_mode) {
    imag_mode = false;
    screen->EFFECT_IMAGE_END;
  }
  if (form_mode) {
    form_mode = false;
    screen->EFFECT_FORM_END;
  }
  if (visited_mode) {
    visited_mode = false;
    EFFECT_VISITED_END;
  }
  if (active_mode) {
    active_mode = false;
    screen->EFFECT_ACTIVE_END;
  }
  if (mark_mode) {
    mark_mode = false;
    screen->EFFECT_MARK_END;
  }
  if (graph_mode) {
    graph_mode = false;
    screen->graphend();
  }
  return rcol - column;
}

static void drawAnchorCursor0(TermScreen *screen,
                              const std::shared_ptr<Buffer> &buf,
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
        redrawLineRegion(screen, buf, l,
                         l->linenumber - tline + buf->layout.rootY, start_pos,
                         end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
      if (active)
        redrawLineRegion(screen, buf, l,
                         l->linenumber - tline + buf->layout.rootY,
                         an->start.pos, an->end.pos);
    }
  }
}

static void drawAnchorCursor(TermScreen *screen,
                             const std::shared_ptr<Buffer> &buf) {
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
    drawAnchorCursor0(screen, buf, buf->layout.href().get(), hseq, prevhseq,
                      tline, eline, 1);
    drawAnchorCursor0(screen, buf, buf->layout.href().get(), hseq, -1, tline,
                      eline, 0);
  }
  if (buf->layout.formitem()) {
    drawAnchorCursor0(screen, buf, buf->layout.formitem().get(), hseq, prevhseq,
                      tline, eline, 1);
    drawAnchorCursor0(screen, buf, buf->layout.formitem().get(), hseq, -1,
                      tline, eline, 0);
  }
  buf->layout.hmarklist()->prevhseq = hseq;
}

void _displayBuffer(TermScreen *screen, int width) {

  auto buf = CurrentTab->currentBuffer();
  if (!buf) {
    return;
  }

  if (buf->layout.width == 0)
    buf->layout.width = width;
  if (buf->layout.height == 0)
    buf->layout.height = screen->LASTLINE() + 1;
  if ((buf->layout.width != width && (buf->res->is_html_type())) ||
      buf->layout.need_reshape) {
    buf->layout.need_reshape = true;
    reshapeBuffer(buf, width);
  }

  // Str *msg;
  int ny = 0;
  auto &layout = buf->layout;

  layout.rootX = 0;
  layout.COLS = screen->COLS() - layout.rootX;
  if (App::instance().nTab() > 1) {
    ny = App::instance().calcTabPos() + 2;
    if (ny > screen->LASTLINE()) {
      ny = screen->LASTLINE();
    }
  }
  if (layout.rootY != ny || layout.LINES != screen->LASTLINE() - ny) {
    layout.rootY = ny;
    layout.LINES = screen->LASTLINE() - ny;
    buf->layout.arrangeCursor();
  }

  App::instance().drawTabs();

  if (cline != buf->layout.topLine || ccolumn != buf->layout.currentColumn) {

    redrawNLine(screen, buf, screen->LASTLINE());

    cline = buf->layout.topLine;
    ccolumn = buf->layout.currentColumn;
  }
  if (buf->layout.topLine == NULL)
    buf->layout.topLine = buf->layout.firstLine;

  drawAnchorCursor(screen, buf);

  auto msg = make_lastline_message(screen, buf);
  if (buf->layout.firstLine == NULL) {
    Strcat_charp(msg, "\tNo Line");
  }

  App::instance().refresh_message();

  screen->standout();
  App::instance().message(msg->ptr, buf->layout.AbsCursorX(),
                          buf->layout.AbsCursorY());
  screen->standend();
  // term_title(buf->layout.title.c_str());
  screen->print();
  screen->cursor({
      .row = buf->layout.AbsCursorY(),
      .col = buf->layout.AbsCursorX(),
  });
  if (buf != save_current_buf) {
    CurrentTab->currentBuffer()->saveBufferInfo();
    save_current_buf = buf;
  }
  if (buf->check_url) {
    chkURLBuffer(buf);
    _displayBuffer(screen, width);
  }
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

static void do_effects(TermScreen *screen, Lineprop m) {
  /* effect end */
  do_effect2(PE_UNDER, ulmode, underline(), screen->underlineend());
  do_effect2(PE_STAND, somode, standout(), screen->standend());
  do_effect2(PE_BOLD, bomode, bold(), screen->boldend());
  do_effect2(PE_EMPH, emph_mode, bold(), screen->boldend());
  do_effect2(PE_ANCHOR, anch_mode, EFFECT_ANCHOR_START,
             screen->EFFECT_ANCHOR_END);
  do_effect2(PE_IMAGE, imag_mode, EFFECT_IMAGE_START, screen->EFFECT_IMAGE_END);
  do_effect2(PE_FORM, form_mode, EFFECT_FORM_START, screen->EFFECT_FORM_END);
  do_effect2(PE_VISITED, visited_mode, EFFECT_VISITED_START,
             EFFECT_VISITED_END);
  do_effect2(PE_ACTIVE, active_mode, EFFECT_ACTIVE_START,
             screen->EFFECT_ACTIVE_END);
  do_effect2(PE_MARK, mark_mode, EFFECT_MARK_START, screen->EFFECT_MARK_END);
  if (graph_mode) {
    screen->graphend();
    graph_mode = false;
  }

  /* effect start */
  do_effect1(PE_UNDER, ulmode, screen->underline(), underlineend());
  do_effect1(PE_STAND, somode, screen->standout(), standend());
  do_effect1(PE_BOLD, bomode, screen->bold(), boldend());
  do_effect1(PE_EMPH, emph_mode, screen->bold(), boldend());
  do_effect1(PE_ANCHOR, anch_mode, screen->EFFECT_ANCHOR_START,
             EFFECT_ANCHOR_END);
  do_effect1(PE_IMAGE, imag_mode, screen->EFFECT_IMAGE_START, EFFECT_IMAGE_END);
  do_effect1(PE_FORM, form_mode, screen->EFFECT_FORM_START, EFFECT_FORM_END);
  do_effect1(PE_VISITED, visited_mode, EFFECT_VISITED_START,
             EFFECT_VISITED_END);
  do_effect1(PE_ACTIVE, active_mode, screen->EFFECT_ACTIVE_START,
             EFFECT_ACTIVE_END);
  do_effect1(PE_MARK, mark_mode, screen->EFFECT_MARK_START, EFFECT_MARK_END);
}
