#include "screen.h"
#include "url.h"
#include "Str.h"
#include "app.h"
#include "myctype.h"
#include "buffer.h"
#include "tabbuffer.h"
#include "http_response.h"
#include <ftxui/screen/screen.hpp>

bool displayLink = false;
int displayLineInfo = false;

void Screen::setupscreen(const RowCol &size) {
  _screen = std::make_shared<ftxui::Screen>(size.col, size.row);
}

void Screen::clear(void) {
  _screen->Clear();
  CurrentMode = ScreenFlags::ASCII;
}

void Screen::clrtoeol(const RowCol &pos) { /* Clear to the end of line */
  if (pos.row >= 0 && pos.row < LINES()) {
    int y = pos.row + 1;
    for (int x = pos.col + 1; x <= COLS(); ++x) {
      _screen->PixelAt(x, y) = {};
    }
  }
}

void Screen::clrtobot_eol(
    const RowCol &pos, const std::function<void(const RowCol &pos)> &callback) {
  clrtoeol(pos);
  int CurLine = pos.row + 1;
  for (; CurLine < LINES(); CurLine++) {
    callback({.row = CurLine, .col = 0});
  }
}

RowCol Screen::addstr(RowCol pos, const char *s) {
  while (*s) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
  }
  return pos;
}

RowCol Screen::addnstr(RowCol pos, const char *s, int n) {
  for (int i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
    i += utf8.view().size();
  }
  return pos;
}

RowCol Screen::addnstr_sup(RowCol pos, const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
    i += utf8.view().size();
  }
  for (; i < n; i++) {
    addch(pos, ' ');
    pos.col += 1;
  }
  return pos;
}

std::string Screen::print() {
  // screen.SetCursor({
  //     .x = CurColumn + 1,
  //     .y = CurLine + 1,
  //     .shape = ftxui::Screen::Cursor::Shape::Hidden,
  // });
  return _screen->ToString();
}

void Screen::toggle_stand(const RowCol &pos) {
  auto &pixel = this->pixel(pos);
  pixel.inverted = !pixel.inverted;
}

void Screen::addmch(const RowCol &pos, const Utf8 &utf8) {
  auto &pixel = this->pixel(pos);
  pixel.character = utf8.view();
}

Str *Screen::make_lastline_link(const std::shared_ptr<Buffer> &buf,
                                const char *title, const char *url) {
  Str *s = NULL;
  Str *u;
  const char *p;
  int l = this->COLS() - 1, i;

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
  auto pu = urlParse(url, buf->res->getBaseURL());
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
    s = Strnew_size(this->COLS());
  i = (l - 2) / 2;
  Strcat_charp_n(s, u->ptr, i);
  Strcat_charp(s, "..");
  i = get_Str_strwidth(u) - (this->COLS() - 1 - get_Str_strwidth(s));
  Strcat_charp(s, &u->ptr[i]);
  return s;
}

Str *Screen::make_lastline_message(const std::shared_ptr<Buffer> &buf) {
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
        s = this->make_lastline_link(buf, p, a ? a->url : NULL);
    }
    if (s) {
      sl = get_Str_strwidth(s);
      if (sl >= this->COLS() - 3)
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
    int l = this->COLS() - 3 - sl;
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

Line *Screen::redrawLine(LineLayout *buf, Line *l, int i) {
  int j, pos, rcol, ncol, delta = 1;
  int column = buf->currentColumn;
  char *p;

  if (l == NULL) {
    return NULL;
  }
  RowCol pixel{.row = i, .col = buf->rootX};
  if (l->len == 0 || l->width() - 1 < column) {
    this->clrtoeolx(pixel);
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
      for (rcol = column; rcol < ncol; rcol++) {
        this->addch(pixel, ' ');
        ++pixel.col;
      }
      continue;
    }
    if (p[j] == '\t') {
      for (; rcol < ncol; rcol++) {
        this->addch(pixel, ' ');
        ++pixel.col;
      }
    } else {
      pixel = this->addnstr(pixel, &p[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    this->standend();
  }
  if (ulmode) {
    ulmode = false;
    this->underlineend();
  }
  if (bomode) {
    bomode = false;
    this->boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    this->boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    this->underlineend();
  }
  if (imag_mode) {
    imag_mode = false;
    this->standend();
  }
  if (form_mode) {
    form_mode = false;
    this->standend();
  }
  if (visited_mode) {
    visited_mode = false;
  }
  if (active_mode) {
    active_mode = false;
    this->boldend();
  }
  if (mark_mode) {
    mark_mode = false;
    this->standend();
  }
  if (graph_mode) {
    graph_mode = false;
    this->graphend();
  }
  if (rcol - column < buf->COLS)
    this->clrtoeolx(pixel);
  return l;
}

void Screen::redrawNLine(const std::shared_ptr<Buffer> &buf, int n) {
  {
    Line *l;
    int i;
    for (i = 0, l = buf->layout.topLine; i < buf->layout.LINES;
         i++, l = l->next) {
      if (i >= buf->layout.LINES - n || i < -n)
        l = this->redrawLine(&buf->layout, l, i + buf->layout.rootY);
      if (l == NULL)
        break;
    }
    if (n > 0) {
      this->clrtobotx({.row = i + buf->layout.rootY, .col = 0});
    }
  }
}

int Screen::redrawLineRegion(const std::shared_ptr<Buffer> &buf, Line *l, int i,
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
        for (rcol = column; rcol < ncol; rcol++) {
          this->addch(pixel, ' ');
          ++pixel.col;
        }
        continue;
      }
      RowCol pixel{.row = i, .col = rcol - column + buf->layout.rootX};
      if (p[j] == '\t') {
        for (; rcol < ncol; rcol++) {
          this->addch(pixel, ' ');
          ++pixel.col;
        }
      } else
        pixel = this->addnstr(pixel, &p[j], delta);
    }
    rcol = ncol;
  }
  if (somode) {
    somode = false;
    this->standend();
  }
  if (ulmode) {
    ulmode = false;
    this->underlineend();
  }
  if (bomode) {
    bomode = false;
    this->boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    this->boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    this->underlineend();
  }
  if (imag_mode) {
    imag_mode = false;
    this->standend();
  }
  if (form_mode) {
    form_mode = false;
    this->standend();
  }
  if (visited_mode) {
    visited_mode = false;
  }
  if (active_mode) {
    active_mode = false;
    this->boldend();
  }
  if (mark_mode) {
    mark_mode = false;
    this->standend();
  }
  if (graph_mode) {
    graph_mode = false;
    this->graphend();
  }
  return rcol - column;
}

void Screen::drawAnchorCursor0(const std::shared_ptr<Buffer> &buf,
                               AnchorList *al, int hseq, int prevhseq,
                               int tline, int eline, int active) {
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
        this->redrawLineRegion(buf, l,
                               l->linenumber - tline + buf->layout.rootY,
                               start_pos, end_pos);
    } else if (prevhseq >= 0 && an->hseq == prevhseq) {
      if (active)
        this->redrawLineRegion(buf, l,
                               l->linenumber - tline + buf->layout.rootY,
                               an->start.pos, an->end.pos);
    }
  }
}

void Screen ::drawAnchorCursor(const std::shared_ptr<Buffer> &buf) {
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
    this->drawAnchorCursor0(buf, buf->layout.href().get(), hseq, prevhseq,
                            tline, eline, 1);
    this->drawAnchorCursor0(buf, buf->layout.href().get(), hseq, -1, tline,
                            eline, 0);
  }
  if (buf->layout.formitem()) {
    this->drawAnchorCursor0(buf, buf->layout.formitem().get(), hseq, prevhseq,
                            tline, eline, 1);
    this->drawAnchorCursor0(buf, buf->layout.formitem().get(), hseq, -1, tline,
                            eline, 0);
  }
  buf->layout.hmarklist()->prevhseq = hseq;
}

std::string Screen::display(int ny, int width, TabBuffer *currentTab) {

  auto buf = currentTab->currentBuffer();
  if (!buf) {
    return {};
  }

  auto &layout = buf->layout;
  layout.rootX = 0;
  layout.COLS = this->COLS() - layout.rootX;
  if (layout.rootY != ny || layout.LINES != this->LASTLINE() - ny) {
    layout.rootY = ny;
    layout.LINES = this->LASTLINE() - ny;
    buf->layout.arrangeCursor();
  }

  App::instance().drawTabs();

  if (cline != buf->layout.topLine || ccolumn != buf->layout.currentColumn) {

    this->redrawNLine(buf, this->LASTLINE());

    cline = buf->layout.topLine;
    ccolumn = buf->layout.currentColumn;
  }
  if (buf->layout.topLine == NULL)
    buf->layout.topLine = buf->layout.firstLine;

  this->drawAnchorCursor(buf);

  auto msg = this->make_lastline_message(buf);
  if (buf->layout.firstLine == NULL) {
    Strcat_charp(msg, "\tNo Line");
  }

  App::instance().refresh_message();

  this->standout();
  App::instance().message(msg->ptr, buf->layout.AbsCursorX(),
                          buf->layout.AbsCursorY());
  this->standend();
  // term_title(buf->layout.title.c_str());
  if (buf != save_current_buf) {
    CurrentTab->currentBuffer()->saveBufferInfo();
    save_current_buf = buf;
  }
  return this->print();
}
