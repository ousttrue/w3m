#include "buffer/display.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/tabbuffer.h"
#include "html/html_renderer.h"
#include "html/map.h"
#include "input/http.h"
#include "input/istream.h"
#include "term/scr.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "text/ctrlcode.h"
#include "text/text.h"
#include "text/utf8.h"
#include <math.h>

int enable_inline_image;
bool displayLink = false;
bool displayLineInfo = false;

static struct Line *cline = nullptr;
static int ccolumn = -1;
static struct Buffer *save_current_buf = nullptr;

static Str make_lastline_link(struct Url *base, const char *title,
                              const char *url) {
  int l = COLS - 1;
  Str s = NULL;
  if (title && *title) {
    s = Strnew_m_charp("[", title, "]", NULL);
    for (auto p = s->ptr; *p; p++) {
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
  auto pu = parseURL2(url, base);
  auto u = parsedURL2Str(&pu);
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
  int i = (l - 2) / 2;
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
  if (ssl_certificate(buf->http_response->stream)) {
    Strcat_charp(msg, "[SSL]");
  }
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

static void drawAnchorCursor0(struct Document *doc, struct AnchorList *al,
                              int hseq, int prevhseq, int tline, int eline,
                              int active) {
  auto l = doc->topLine;
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
      // clear
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
        render_line_region(&doc->viewport, l,
                           l->linenumber - tline + doc->viewport.rootY,
                           start_pos, end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
      if (active)
        // highlight
        render_line_region(&doc->viewport, l,
                           l->linenumber - tline + doc->viewport.rootY,
                           an->start.pos, an->end.pos);
    }
  }
}

static void drawAnchorCursor(struct Document *doc) {
  if (!doc->firstLine || !doc->hmarklist)
    return;
  if (!doc->href && !doc->formitem)
    return;

  auto an = retrieveCurrentAnchor(doc);
  if (!an)
    an = retrieveCurrentMap(doc);

  int hseq;
  if (an)
    hseq = an->hseq;
  else
    hseq = -1;

  auto tline = doc->topLine->linenumber;
  auto eline = tline + doc->viewport.LINES;
  auto prevhseq = doc->hmarklist->prevhseq;
  // if (doc->href) {
  //   drawAnchorCursor0(doc, doc->href, hseq, prevhseq, tline, eline, 1);
  //   drawAnchorCursor0(doc, doc->href, hseq, -1, tline, eline, 0);
  // }
  // if (doc->formitem) {
  //   drawAnchorCursor0(doc, doc->formitem, hseq, prevhseq, tline, eline, 1);
  //   drawAnchorCursor0(doc, doc->formitem, hseq, -1, tline, eline, 0);
  // }
  doc->hmarklist->prevhseq = hseq;
}

static void render_document(struct Document *doc) {
  if (nTab > 1) {
    scr_move(0, 0);
    scr_clrtoeolx();
    for (auto t = FirstTab; t; t = t->nextTab) {
      scr_move(t->y, t->x1);
      if (t == CurrentTab)
        scr_bold();
      scr_addch('[');
      int l = t->x2 - t->x1 - 1 -
              utf8str_width((const uint8_t *)t->currentBuffer->buffername);
      if (l < 0)
        l = 0;
      if (l / 2 > 0)
        scr_addnstr_sup(" ", l / 2);
      if (t == CurrentTab)
        scr_bold();
      scr_addnstr(t->currentBuffer->buffername, t->x2 - t->x1 - l);
      if (t == CurrentTab)
        scr_boldend();
      if ((l + 1) / 2 > 0)
        scr_addnstr_sup(" ", (l + 1) / 2);
      scr_move(t->y, t->x2);
      scr_addch(']');
      if (t == CurrentTab)
        scr_boldend();
    }
    scr_move(LastTab->y + 1, 0);
    for (int i = 0; i < COLS; i++)
      scr_addch('~');
  }

  struct Line *l = doc->topLine;
  int i = 0;
  for (; i < doc->viewport.LINES; i++, l = l->next) {
    if (i >= doc->viewport.LINES - LASTLINE || i < -LASTLINE) {
      // l = redrawLine(doc, l, i + doc->viewport.rootY);
      if (l) {
        scr_move(i, 0);
        if (showLineNum) {
          char tmp[16];
          if (!doc->viewport.rootX) {
            if (doc->lastLine->real_linenumber > 0)
              doc->viewport.rootX =
                  (int)(log(doc->lastLine->real_linenumber + 0.1) / log(10)) +
                  2;
            if (doc->viewport.rootX < 5)
              doc->viewport.rootX = 5;
            if (doc->viewport.rootX > COLS)
              doc->viewport.rootX = COLS;
            doc->viewport.COLS = COLS - doc->viewport.rootX;
          }
          if (l->real_linenumber && !l->bpos)
            sprintf(tmp, "%*ld:", doc->viewport.rootX - 1, l->real_linenumber);
          else
            sprintf(tmp, "%*s ", doc->viewport.rootX - 1, "");
          scr_addstr(tmp);
        }
        l = render_line(l, &doc->viewport, i);
      }
    }
    if (l == NULL)
      break;
  }
  if (LASTLINE > 0) {
    scr_move(i + doc->viewport.rootY, 0);
    scr_clrtobotx();
  }
}

// tabs/standout
// addressbar(url)
// title/standout
// document
// status(standout)
// msg
void displayBuffer(struct Buffer *buf, enum DisplayMode mode) {
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
       (is_html_type(buf->document->type) || FoldLine)) ||
      buf->document->need_reshape) {
    buf->document->need_reshape = true;
    buf->document =
        reshapeBuffer(buf->document, buf->http_response->content_charset);
  }
  if (showLineNum) {
    if (buf->document->lastLine && buf->document->lastLine->real_linenumber > 0)
      buf->document->viewport.rootX =
          (int)(log(buf->document->lastLine->real_linenumber + 0.1) / log(10)) +
          2;
    if (buf->document->viewport.rootX < 5)
      buf->document->viewport.rootX = 5;
    if (buf->document->viewport.rootX > COLS)
      buf->document->viewport.rootX = COLS;
  } else
    buf->document->viewport.rootX = 0;
  buf->document->viewport.COLS = COLS - buf->document->viewport.rootX;

  int ny = 0;
  if (nTab > 1) {
    if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
      calcTabPos();
    ny = LastTab->y + 2;
    if (ny > LASTLINE)
      ny = LASTLINE;
  }
  if (buf->document->viewport.rootY != ny ||
      buf->document->viewport.LINES != LASTLINE - ny) {
    buf->document->viewport.rootY = ny;
    buf->document->viewport.LINES = LASTLINE - ny;
    arrangeCursor(buf->document);
    mode = B_REDRAW_IMAGE;
  }
  if (mode == B_FORCE_REDRAW || mode == B_SCROLL || mode == B_REDRAW_IMAGE ||
      cline != buf->document->topLine ||
      ccolumn != buf->document->viewport.currentColumn) {
    {
      render_document(buf->document);
    }
    cline = buf->document->topLine;
    ccolumn = buf->document->viewport.currentColumn;
  }
  if (buf->document->topLine == NULL) {
    buf->document->topLine = buf->document->firstLine;
  }

  drawAnchorCursor(buf->document);

  // message
  auto msg = make_lastline_message(buf);
  if (buf->document->firstLine == NULL) {
    Strcat_charp(msg, "\tNo Line");
  }
  // term_show_delayed_message();
  scr_standout();
  scr_message(msg->ptr,
              buf->document->viewport.cursorX + buf->document->viewport.rootX,
              buf->document->viewport.cursorY + buf->document->viewport.rootY);
  scr_standend();

  term_title(buf->buffername);
  term_refresh();

  if (buf != save_current_buf) {
    saveBufferInfo();
    save_current_buf = buf;
  }

  if (mode == B_FORCE_REDRAW && (buf->document->check_url & CHK_URL)) {
    chkURLBuffer(buf->document);
    displayBuffer(buf, B_NORMAL);
  }
}

void displayInvalidate() { displayBuffer(Currentbuf, B_NORMAL); }
