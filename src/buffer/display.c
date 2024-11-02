#include "buffer/display.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/tabbuffer.h"
#include "html/html_readbuffer.h"
#include "html/map.h"
#include "input/content.h"
#include "input/istream.h"
#include "term/scr.h"
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

static Str make_lastline_link(struct Url *base, const char *title,
                              const char *url) {
  auto size = term_size();
  int l = size.cols - 1;
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
    s = Strnew_size(size.cols);
  int i = (l - 2) / 2;
  Strcat_charp_n(s, u->ptr, i);
  Strcat_charp(s, "..");
  i = utf8str_width((const uint8_t *)u->ptr) -
      (size.cols - 1 - utf8str_width((const uint8_t *)s->ptr));
  Strcat_charp(s, &u->ptr[i]);
  return s;
}

static Str make_lastline_message(struct Buffer *buf, struct TermSize size) {
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
      if (sl >= size.cols - 3)
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
  // if (buf->http_response && ssl_certificate(buf->http_response->stream)) {
  //   Strcat_charp(msg, "[SSL]");
  // }
  Strcat_charp(msg, " <");
  // Strcat_charp(msg, buf->buffername);

  if (s) {
    int l = size.cols - 3 - sl;
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
              utf8str_width((const uint8_t *)t->currentBuffer->document->title);
      if (l < 0)
        l = 0;
      if (l / 2 > 0)
        scr_addnstr_sup(" ", l / 2);
      if (t == CurrentTab)
        scr_bold();
      scr_addnstr(t->currentBuffer->document->title, t->x2 - t->x1 - l);
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
    for (int i = 0; i < term_size().cols; i++)
      scr_addch('~');
  }

  struct Line *l = doc->topLine;
  int i = 0;
  for (; i < doc->viewport.LINES; i++, l = l->next) {
    if (i >= doc->viewport.LINES - term_size().lines - 1 ||
        i < -(term_size().lines - 1)) {
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
            if (doc->viewport.rootX > term_size().cols)
              doc->viewport.rootX = term_size().cols;
            doc->viewport.COLS = term_size().cols - doc->viewport.rootX;
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
  if (term_size().lines - 1 > 0) {
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
void display(struct Buffer *buf, struct TermSize size) {
  if (!buf)
    return;
  auto doc = buf->document;
  if (!doc) {
    return;
  }

  if (doc->topLine == NULL && readBufferCache(doc) == 0) { /* clear_buffer */
  }

  // if ((buf->document->width != INIT_BUFFER_WIDTH &&
  //      (is_html_type(buf->document->type) || FoldLine)) ||
  //     buf->document->need_reshape) {
  //   buf->document->need_reshape = true;
  //   buf->document =
  //       reshapeBuffer(buf->document, buf->http_response ?
  //       buf->http_response->content_charset : CHARSET_UTF8);
  // }
  if (showLineNum) {
    if (doc->lastLine && doc->lastLine->real_linenumber > 0)
      doc->viewport.rootX =
          (int)(log(doc->lastLine->real_linenumber + 0.1) / log(10)) + 2;
    if (doc->viewport.rootX < 5)
      doc->viewport.rootX = 5;
    if (doc->viewport.rootX > size.cols)
      doc->viewport.rootX = size.cols;
  } else
    doc->viewport.rootX = 0;
  doc->viewport.COLS = size.cols - doc->viewport.rootX;

  int ny = 0;
  if (nTab > 1) {
    // if (mode == B_FORCE_REDRAW || mode == B_REDRAW_IMAGE)
    calcTabPos(size);
    ny = LastTab->y + 2;
    if (ny > size.lines - 1)
      ny = size.lines - 1;
  }
  if (doc->viewport.rootY != ny || doc->viewport.LINES != size.lines - 1 - ny) {
    doc->viewport.rootY = ny;
    doc->viewport.LINES = size.lines - 1 - ny;
    arrangeCursor(doc);
  }

  render_document(doc);
  cline = doc->topLine;
  ccolumn = doc->viewport.currentColumn;

  if (doc->topLine == NULL) {
    doc->topLine = doc->firstLine;
  }

  drawAnchorCursor(doc);

  // message
  auto msg = make_lastline_message(buf, size);
  if (doc->firstLine == NULL) {
    Strcat_charp(msg, "\tNo Line");
  }
  // term_show_delayed_message();
  scr_standout();
  scr_message(msg->ptr, doc->viewport.cursorX + doc->viewport.rootX,
              doc->viewport.cursorY + doc->viewport.rootY);
  scr_standend();
}

#define _INIT_BUFFER_WIDTH ()
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)

/*
 * Reshape HTML buffer
 */
void reshapeBuffer(struct Buffer *buf, struct TermSize size) {
  if (buf->content->sourcefile == NULL) {
    return;
  }

  if (buf->document) {
    if (!buf->document->need_reshape) {
      return;
    }
    buf->document->need_reshape = false;
  }

  auto stream = examineFile(buf->content->sourcefile);
  if (!stream) {
    return;
  }

  Str html = Strnew();
  Str line;
  while ((line = StrmyISgets(stream))->length) {
    Strcat(html, line);
  }
  ISclose(stream);

  auto doc = buf->document;
  if (is_html_type(buf->content->content_type)) {
    buf->document = renderHTML(size.cols, html->ptr, buf->content->url,
                               buf->content->charset);
  } else {
    buf->document = loadText(size.cols, html->ptr);
  }
  buf->document->height = size.lines;

  // buf->document->width = size.cols - (showLineNum ? 6 : 1);
  // if (buf->document->width < 0) {
  //   buf->document->width = 0;
  // }
  // buf->document->height = size.lines;
  // struct Document sbuf;
  // copyBuffer(&sbuf, buf->document);
  // clearBuffer(buf->document);
  // buf->document->href = NULL;
  // buf->document->name = NULL;
  // buf->document->img = NULL;
  // buf->document->formitem = NULL;
  // buf->document->formlist = NULL;
  // buf->document->linklist = NULL;
  // buf->document->maplist = NULL;
  // if (buf->document->hmarklist)
  //   buf->document->hmarklist->nmark = 0;
  // if (buf->document->imarklist)
  //   buf->document->imarklist->nmark = 0;

  if (doc) {
    buf->document->viewport = doc->viewport;
    if (buf->document->firstLine && doc->firstLine) {
      struct Line *cur = doc->currentLine;

      buf->document->viewport.pos = doc->viewport.pos + cur->bpos;
      while (cur->bpos && cur->prev)
        cur = cur->prev;
      if (cur->real_linenumber > 0)
        gotoRealLine(buf->document, cur->real_linenumber);
      else
        gotoLine(buf->document, cur->linenumber);
      int n = (buf->document->currentLine->linenumber -
           buf->document->topLine->linenumber) -
          (cur->linenumber - doc->topLine->linenumber);
      if (n) {
        buf->document->topLine =
            lineSkip(&buf->document->viewport, buf->document->topLine,
                     buf->document->lastLine, n, false);
        if (cur->real_linenumber > 0)
          gotoRealLine(buf->document, cur->real_linenumber);
        else
          gotoLine(buf->document, cur->linenumber);
      }
      buf->document->viewport.pos -= buf->document->currentLine->bpos;
      if (FoldLine && !is_html_type(buf->content->content_type))
        buf->document->viewport.currentColumn = 0;
      else
        buf->document->viewport.currentColumn = doc->viewport.currentColumn;
      arrangeCursor(buf->document);
    }
    if (buf->document->check_url & CHK_URL)
      chkURLBuffer(buf->document);
    formResetBuffer(buf->document, doc->formitem);
  }
}
