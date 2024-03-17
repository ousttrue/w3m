#include "line_layout.h"
#include "myctype.h"
#include "url.h"
#include "html/form.h"
#include "html/form_item.h"
#include "regex.h"
#include "alloc.h"
#include "utf8.h"

bool FoldTextarea = false;

LineLayout::LineLayout() {}

void LineLayout::clearBuffer() {
  data.clear();
  visual = {0};
}

/* go to the next downward/upward anchor */
void LineLayout::nextY(int d, int n) {
  if (empty()) {
    return;
  }

  auto hl = this->data._hmarklist;
  if (hl->size() == 0) {
    return;
  }

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = this->retrieveCurrentForm();
  }

  int x = this->cursorPos();
  int y = this->visual.cursor.row + d;
  Anchor *pan = nullptr;
  int hseq = -1;
  for (int i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = nullptr;
    for (; y >= 0 && y <= linenumber(this->lastLine()); y += d) {
      if (this->data._href) {
        an = this->data._href->retrieveAnchor(y, x);
      }
      if (!an && this->data._formitem) {
        an = this->data._formitem->retrieveAnchor(y, x);
      }
      if (an && hseq != abs(an->hseq)) {
        pan = an;
        break;
      }
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  this->visual.cursorRow(pan->start.line);
}

/* go to the next left/right anchor */
void LineLayout::nextX(int d, int dy, int n) {
  if (empty())
    return;

  auto hl = this->data._hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (!an) {
    an = retrieveCurrentForm();
  }

  auto l = this->currentLine();
  auto x = this->cursorPos();
  auto y = linenumber(l);
  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = nullptr;
    while (1) {
      for (; x >= 0 && x < l->len(); x += d) {
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(y, x);
        }
        if (!an && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(y, x);
        }
        if (an) {
          pan = an;
          break;
        }
      }
      if (!dy || an)
        break;
      (dy > 0) ? ++l : --l;
      if (!l)
        break;
      x = (d > 0) ? 0 : l->len() - 1;
      y = linenumber(l);
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  this->visual.cursorRow(y);
  this->cursorPos(pan->start.pos);
}

/* go to the previous anchor */
void LineLayout::_prevA(std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->data._hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  int y = this->visual.cursor.row;
  int x = this->cursorPos();

  for (int i = 0; i < n; i++) {
    auto pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq - 1;
      do {
        if (hseq < 0) {
          an = pan;
          goto _end;
        }
        auto po = hl->get(hseq);
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(po->line, po->pos);
        }
        if (an == nullptr && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq--;
      } while (an == nullptr || an == pan);
    } else {
      an = this->data._href->closest_prev_anchor((Anchor *)nullptr, x, y);
      an = this->data._formitem->closest_prev_anchor(an, x, y);
      if (an == nullptr) {
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
    }
  }

_end:
  if (an == nullptr || an->hseq < 0)
    return;

  {
    auto po = hl->get(an->hseq);
    this->visual.cursorRow(po->line);
    this->cursorPos(po->pos);
  }
}

/* go to the next anchor */
void LineLayout::_nextA(std::optional<Url> baseUrl, int n) {
  if (empty())
    return;

  auto hl = this->data._hmarklist;
  if (hl->size() == 0)
    return;

  auto an = this->retrieveCurrentAnchor();
  if (an == nullptr) {
    an = this->retrieveCurrentForm();
  }

  auto y = visual.cursor.row;
  auto x = this->cursorPos();

  Anchor *pan = nullptr;
  for (int i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq + 1;
      do {
        if (hseq >= (int)hl->size()) {
          an = pan;
          goto _end;
        }
        auto po = hl->get(hseq);
        if (this->data._href) {
          an = this->data._href->retrieveAnchor(po->line, po->pos);
        }
        if (an == nullptr && this->data._formitem) {
          an = this->data._formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq++;
      } while (an == nullptr || an == pan);
    } else {
      an = this->data._href->closest_next_anchor((Anchor *)nullptr, x, y);
      an = this->data._formitem->closest_next_anchor(an, x, y);
      if (an == nullptr) {
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
    }
  }

_end:
  if (an == nullptr || an->hseq < 0)
    return;

  auto po = hl->get(an->hseq);
  this->visual.cursorRow(po->line);
  this->cursorPos(po->pos);
}

int LineLayout::prev_nonnull_line(Line *line) {
  Line *l;
  for (l = line; !isNull(l) && l->len() == 0; --l)
    ;
  if (l == nullptr || l->len() == 0)
    return -1;

  this->visual.cursor.row = linenumber(l);
  if (l != line)
    this->cursorPos(this->currentLine()->len());
  return 0;
}

int LineLayout::next_nonnull_line(Line *line) {
  Line *l;
  for (l = line; !isNull(l) && l->len() == 0; ++l)
    ;
  if (l == nullptr || l->len() == 0)
    return -1;

  this->visual.cursor.row = linenumber(l);
  if (l != line)
    this->visual.cursor.col = 0;
  return 0;
}

/* Go to specified line */
void LineLayout::_goLine(const char *l, int prec_num) {
  if (l == nullptr || *l == '\0' || this->currentLine() == nullptr) {
    return;
  }

  this->visual.cursor.col = 0;
  if (((*l == '^') || (*l == '$')) && prec_num) {
    this->visual.cursorRow(prec_num);
  } else if (*l == '^') {
    this->visual.cursorHome();
  } else if (*l == '$') {
    // this->_scroll.row = linenumber(this->lastLine()) - (this->LINES + 1) / 2;
    this->visual.cursor.row = linenumber(this->lastLine());
  } else {
    this->visual.cursorRow(atoi(l));
  }
}

std::string LineLayout::getCurWord(int *spos, int *epos) const {
  auto l = this->currentLine();
  if (l == nullptr)
    return {};

  *spos = 0;
  *epos = 0;
  auto p = l->lineBuf();
  auto e = this->cursorPos();
  while (e > 0 && !is_wordchar(p[e]))
    prevChar(e, l);
  if (!is_wordchar(p[e]))
    return {};
  int b = e;
  while (b > 0) {
    int tmp = b;
    prevChar(tmp, l);
    if (!is_wordchar(p[tmp]))
      break;
    b = tmp;
  }
  while (e < l->len() && is_wordchar(p[e])) {
    e++;
  }
  *spos = b;
  *epos = e;
  return {p + b, p + e};
}

/* mark URL-like patterns as anchors */
static const char *url_like_pat[] = {
    "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
    "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
    "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
    "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
#ifdef INET6
    "https?://[a-zA-Z0-9:%\\-\\./"
    "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./"
    "?=~_\\&+@#,\\$;]*",
    "ftp://[a-zA-Z0-9:%\\-\\./"
    "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif /* INET6 */
    nullptr};
void LineLayout::chkURLBuffer() {
  for (int i = 0; url_like_pat[i]; i++) {
    this->data.reAnchor(topLine(), url_like_pat[i]);
  }
  this->check_url = true;
}

void LineLayout::reshape(int width) {
  if (!this->data.need_reshape) {
    return;
  }
  // this->data.need_reshape = false;
  // this->width = width;
  //
  // this->height = App::instance().LASTLINE() + 1;
  // if (!empty() && !sbuf.empty()) {
  //   const Line *cur = sbuf.currentLine();
  //   this->pos = sbuf.pos;
  //   this->cursorRow(sbuf.linenumber(cur));
  //   int n = cursor.row - (sbuf.linenumber(cur) - sbuf._topLine);
  //   if (n) {
  //     this->_topLine += n;
  //     this->cursorRow(sbuf.linenumber(cur));
  //   }
  //   this->currentColumn = sbuf.currentColumn;
  //   this->arrangeCursor();
  // }
  // if (this->check_url) {
  //   this->chkURLBuffer();
  // }
  // this->formResetBuffer(sbuf.data._formitem.get());
}

Anchor *LineLayout::retrieveCurrentAnchor() {
  if (!this->currentLine() || !this->data._href)
    return NULL;
  return this->data._href->retrieveAnchor(visual.cursor.row, this->cursorPos());
}

Anchor *LineLayout::retrieveCurrentImg() {
  if (!this->currentLine() || !this->data._img)
    return NULL;
  return this->data._img->retrieveAnchor(visual.cursor.row, this->cursorPos());
}

FormAnchor *LineLayout::retrieveCurrentForm() {
  if (!this->currentLine() || !this->data._formitem)
    return NULL;
  return this->data._formitem->retrieveAnchor(visual.cursor.row,
                                              this->cursorPos());
}

void LineLayout::formResetBuffer(const AnchorList<FormAnchor> *formitem) {

  if (this->data._formitem == NULL || formitem == NULL)
    return;

  for (size_t i = 0; i < this->data._formitem->size() && i < formitem->size();
       i++) {
    auto a = &this->data._formitem->anchors[i];
    if (a->y != a->start.line)
      continue;

    auto f1 = a->formItem;
    auto f2 = formitem->anchors[i].formItem;
    if (f1->type != f2->type ||
        strcmp(((f1->name == NULL) ? "" : f1->name->ptr),
               ((f2->name == NULL) ? "" : f2->name->ptr)))
      break; /* What's happening */
    switch (f1->type) {
    case FORM_INPUT_TEXT:
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_FILE:
    case FORM_TEXTAREA:
      f1->value = f2->value;
      f1->init_value = f2->init_value;
      break;
    case FORM_INPUT_CHECKBOX:
    case FORM_INPUT_RADIO:
      f1->checked = f2->checked;
      f1->init_checked = f2->init_checked;
      break;
    case FORM_SELECT:
      break;
    default:
      continue;
    }

    this->formUpdateBuffer(a);
  }
}

static int form_update_line(Line *line, char **str, int spos, int epos,
                            int width, int newline, int password) {
  int c_len = 1, c_width = 1, w, i, len, pos;
  char *p, *buf;
  Lineprop c_type, effect, *prop;

  for (p = *str, w = 0, pos = 0; *p && w < width;) {
    c_type = get_mctype(p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        w++;
        pos++;
      }
    } else if (password) {
      w += c_width;
      pos += c_width;
      w += c_width;
      pos += c_len;
    }
    p += c_len;
  }
  pos += width - w;

  len = line->len() + pos + spos - epos;
  buf = (char *)New_N(char, len + 1);
  buf[len] = '\0';
  prop = (Lineprop *)New_N(Lineprop, len);
  memcpy(buf, line->lineBuf(), spos * sizeof(char));
  memcpy(prop, line->propBuf(), spos * sizeof(Lineprop));

  effect = CharEffect(line->propBuf()[spos]);
  for (p = *str, w = 0, pos = spos; *p && w < width;) {
    c_type = get_mctype(p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        buf[pos] = password ? '*' : ' ';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else if (password) {
      for (i = 0; i < c_width; i++) {
        buf[pos] = '*';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else {
      buf[pos] = *p;
      prop[pos] = effect | c_type;
      pos++;
      w += c_width;
    }
    p += c_len;
  }
  for (; w < width; w++) {
    buf[pos] = ' ';
    prop[pos] = effect | PC_ASCII;
    pos++;
  }
  if (newline) {
    if (!FoldTextarea) {
      while (*p && *p != '\r' && *p != '\n')
        p++;
    }
    if (*p == '\r')
      p++;
    if (*p == '\n')
      p++;
  }
  *str = p;

  memcpy(&buf[pos], &line->lineBuf()[epos],
         (line->len() - epos) * sizeof(char));
  memcpy(&prop[pos], &line->propBuf()[epos],
         (line->len() - epos) * sizeof(Lineprop));

  line->assign(buf, prop, len);

  return pos;
}
void LineLayout::formUpdateBuffer(FormAnchor *a) {
  char *p;
  int spos, epos, rows, c_rows, pos, col = 0;
  Line *l;

  // LineLayout save = *this;
  this->visual.cursorRow(a->start.line);
  auto form = a->formItem;
  switch (form->type) {
  case FORM_TEXTAREA:
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_PASSWORD:
  case FORM_INPUT_CHECKBOX:
  case FORM_INPUT_RADIO:
    spos = a->start.pos;
    epos = a->end.pos;
    break;
  default:
    spos = a->start.pos + 1;
    epos = a->end.pos - 1;
  }

  switch (form->type) {
  case FORM_INPUT_CHECKBOX:
  case FORM_INPUT_RADIO:
    if (this->currentLine() == NULL || spos >= this->currentLine()->len() ||
        spos < 0)
      break;
    if (form->checked)
      this->currentLine()->lineBuf(spos, '*');
    else
      this->currentLine()->lineBuf(spos, ' ');
    break;
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_PASSWORD:
  case FORM_TEXTAREA: {
    if (!form->value)
      break;
    p = form->value->ptr;

    l = this->currentLine();
    if (!l)
      break;
    if (form->type == FORM_TEXTAREA) {
      int n = a->y - this->visual.cursor.row;
      if (n > 0)
        for (; !this->isNull(l) && n; --l, n--)
          ;
      else if (n < 0)
        for (; !this->isNull(l) && n; --l, n++)
          ;
      if (!l)
        break;
    }
    rows = form->rows ? form->rows : 1;
    col = l->bytePosToColumn(a->start.pos);
    for (c_rows = 0; c_rows < rows; c_rows++, ++l) {
      if (this->isNull(l))
        break;
      if (rows > 1 && this->data._formitem) {
        pos = l->columnPos(col);
        a = this->data._formitem->retrieveAnchor(this->linenumber(l), pos);
        if (a == NULL)
          break;
        spos = a->start.pos;
        epos = a->end.pos;
      }
      if (a->start.line != a->end.line || spos > epos || epos >= l->len() ||
          spos < 0 || epos < 0 || l->bytePosToColumn(epos) < col)
        break;
      pos = form_update_line(l, &p, spos, epos, l->bytePosToColumn(epos) - col,
                             rows > 1, form->type == FORM_INPUT_PASSWORD);
      if (pos != epos) {
        this->data._href->shiftAnchorPosition(this->data._hmarklist.get(),
                                              a->start.line, spos, pos - epos);
        this->data._name->shiftAnchorPosition(this->data._hmarklist.get(),
                                              a->start.line, spos, pos - epos);
        this->data._img->shiftAnchorPosition(this->data._hmarklist.get(),
                                             a->start.line, spos, pos - epos);
        this->data._formitem->shiftAnchorPosition(
            this->data._hmarklist.get(), a->start.line, spos, pos - epos);
      }
    }
    break;
  }

  default:
    break;
  }
  // *this = save;
}

// class ActiveAnchorDrawer {
//
//   Screen *_screen;
//   RowCol _root;
//   LineLayout *_layout;
//
// public:
//   ActiveAnchorDrawer(Screen *screen, const RowCol &root, LineLayout *layout)
//       : _screen(screen), _root(root), _layout(layout) {}
//
//   void drawAnchorCursor() {
//     int hseq = -1;
//     auto an = _layout->retrieveCurrentAnchor();
//     if (an) {
//       hseq = an->hseq;
//     }
//     int tline = _layout->linenumber(_layout->topLine);
//     int eline = tline + _layout->LINES;
//     int prevhseq = _layout->data._hmarklist->prevhseq;
//
//     if (_layout->data._href) {
//       this->drawAnchorCursor0(_layout->data._href.get(), hseq, prevhseq,
//       tline,
//                               eline, true);
//       this->drawAnchorCursor0(_layout->data._href.get(), hseq, -1, tline,
//       eline,
//                               false);
//     }
//     if (_layout->data._formitem) {
//       this->drawAnchorCursor0(_layout->data._formitem.get(), hseq, prevhseq,
//                               tline, eline, true);
//       this->drawAnchorCursor0(_layout->data._formitem.get(), hseq, -1, tline,
//                               eline, false);
//     }
//     _layout->data._hmarklist->prevhseq = hseq;
//   }
//
// private:
//   template <typename T>
//   void drawAnchorCursor0(AnchorList<T> *al, int hseq, int prevhseq, int
//   tline,
//                          int eline, int active) {
//     auto l = _layout->topLine;
//     for (size_t j = 0; j < al->size(); j++) {
//       auto an = &al->anchors[j];
//       if (an->start.line < tline)
//         continue;
//       if (an->start.line >= eline)
//         return;
//       for (;; ++l) {
//         if (_layout->isNull(l))
//           return;
//         if (_layout->linenumber(l) == an->start.line)
//           break;
//       }
//       if (hseq >= 0 && an->hseq == hseq) {
//         int start_pos = an->start.pos;
//         int end_pos = an->end.pos;
//         for (int i = an->start.pos; i < an->end.pos; i++) {
//           if (l->propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM)) {
//             if (active)
//               l->propBuf[i] |= PE_ACTIVE;
//             else
//               l->propBuf[i] &= ~PE_ACTIVE;
//           }
//         }
//         if (active && start_pos < end_pos) {
//           _screen->redrawLineRegion(_root, _layout, l,
//                                     _layout->linenumber(l) - tline +
//                                     _root.row, start_pos, end_pos);
//         }
//       } else if (prevhseq >= 0 && an->hseq == prevhseq) {
//         if (active) {
//           _screen->redrawLineRegion(_root, _layout, l,
//                                     _layout->linenumber(l) - tline +
//                                     _root.row, an->start.pos, an->end.pos);
//         }
//       }
//     }
//   }
// };

void LineLayout::setupscreen(const RowCol &size) {
  _screen = std::make_shared<ftxui::Screen>(size.col, size.row);
}

void LineLayout::clear(void) {
  _screen->Clear();
  CurrentMode = ScreenFlags::ASCII;
}

void LineLayout::clrtoeol(const RowCol &pos) { /* Clear to the end of line */
  if (pos.row >= 0 && pos.row < LINES()) {
    int y = pos.row + 1;
    for (int x = pos.col + 1; x <= COLS(); ++x) {
      _screen->PixelAt(x, y) = {};
    }
  }
}

void LineLayout::clrtobot_eol(
    const RowCol &pos, const std::function<void(const RowCol &pos)> &callback) {
  clrtoeol(pos);
  int CurLine = pos.row + 1;
  for (; CurLine < LINES(); CurLine++) {
    callback({.row = CurLine, .col = 0});
  }
}

RowCol LineLayout::addstr(RowCol pos, const char *s) {
  while (*s) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8);
    pos.col += utf8.width();
    s += utf8.view().size();
  }
  return pos;
}

RowCol LineLayout::addnstr(RowCol pos, const char *s, int n, ScreenFlags mode) {
  for (int i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8, mode);
    pos.col += utf8.width();
    s += utf8.view().size();
    i += utf8.view().size();
  }
  return pos;
}

RowCol LineLayout::addnstr_sup(RowCol pos, const char *s, int n) {
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

void LineLayout::toggle_stand(const RowCol &pos) {
  auto &pixel = this->pixel(pos);
  pixel.inverted = !pixel.inverted;
}

void LineLayout::addmch(const RowCol &pos, const Utf8 &utf8, ScreenFlags mode) {
  auto &pixel = this->pixel(pos);
  pixel.character = utf8.view();
  pixel.bold = (int)(mode & ScreenFlags::BOLD);
  pixel.underlined = (int)(mode & ScreenFlags::UNDERLINE);
}

// std::string LineLayout::str(const RowCol &root, LineLayout *layout) {
//   // draw lines
//   int i = 0;
//   int height = box_.y_max - box_.y_min + 1;
//   int width = box_.x_max - box_.x_min + 1;
//   for (auto l = layout->_scroll.row;
//        i < height && l < (int)layout->data.lines.size(); ++i, ++l) {
//     auto pixel = this->redrawLine(
//         {
//             .row = i + root.row,
//             .col = root.col,
//         },
//         l, width, layout->_scroll.col);
//     this->clrtoeolx(pixel);
//   }
//   // clear remain
//   this->clrtobotx({
//       .row = i + root.row,
//       .col = 0,
//   });
//   // cline = layout->_topLine;
//   // ccolumn = layout->currentColumn;
//
//   // highlight active anchor
//   // if (!layout->empty() && layout->data._hmarklist) {
//   //   if (layout->data._href || layout->data._formitem) {
//   //     class ActiveAnchorDrawer d(this, root, layout);
//   //     d.drawAnchorCursor();
//   //   }
//   // }
//
//   // toStr
//   return this->_screen->ToString();
// }

void LineLayout::Render(ftxui::Screen &screen) {
  //
  // render to _screen
  //
  _screen = std::make_shared<ftxui::Screen>(box_.x_max - box_.x_min + 1,
                                            box_.y_max - box_.y_min + 1);
  auto l = this->visual.scroll.row;
  int i = 0;
  int height = box_.y_max - box_.y_min + 1;
  int width = box_.x_max - box_.x_min + 1;

  visual.size = {.row = height, .col = width};

  for (; i < height; ++i, ++l) {
    auto pixel = this->redrawLine(
        {
            .row = i,
            .col = 0,
        },
        l, width, this->visual.scroll.col);
    this->clrtoeolx(pixel);
  }
  // clear remain
  this->clrtobotx({
      .row = i,
      .col = 0,
  });
  // cline = layout->_topLine;
  // ccolumn = layout->currentColumn;

  //
  // _screen to screen
  //
  int x = box_.x_min;
  const int y = box_.y_min;
  if (y > box_.y_max) {
    return;
  }
  // draw line
  for (int row = 0; row < screen.dimy(); ++row) {
    for (int col = 0; col < screen.dimx(); ++col) {
      auto p = _screen->PixelAt(col, row);
      screen.PixelAt(x + col, y + row) = p;
    }
  }
}

static ScreenFlags propToFlag(Lineprop prop) {
  ScreenFlags flag =
      (prop & PE_BOLD ? ScreenFlags::BOLD : ScreenFlags::NORMAL) |
      (prop & PE_UNDER ? ScreenFlags::UNDERLINE : ScreenFlags::NORMAL) |
      (prop & PE_STAND ? ScreenFlags::STANDOUT : ScreenFlags::NORMAL);
  return flag;
}

RowCol LineLayout::redrawLine(RowCol pixel, int _l, int cols, int scrollLeft) {
  if (_l < 0 || _l >= (int)this->data.lines.size()) {
    return pixel;
  }

  auto l = &this->data.lines[_l];
  int pos = l->columnPos(pixel.col);
  auto p = l->lineBuf() + pos;
  auto pr = l->propBuf() + pos;
  int col = pixel.col; // l->bytePosToColumn(pos);

  if (col < scrollLeft) {
    // TODO:
    assert(false);
    // for (col = scrollLeft; col < nextCol; col++) {
    //   this->addch(pixel, ' ');
    //   ++pixel.col;
    // }
  }

  for (int j = 0; pos + j < l->len();) {
    int delta = get_mclen(&p[j]);
    int nextCol = l->bytePosToColumn(pos + j + delta);
    if (nextCol - scrollLeft > cols) {
      break;
    }

    if (p[j] == '\t') {
      for (; col < nextCol; col++) {
        this->addch(pixel, ' ');
        ++pixel.col;
      }
    } else {
      pixel = this->addnstr(pixel, &p[j], delta, propToFlag(pr[j]));
    }
    col = nextCol;
    j += delta;
  }

  if (this->somode) {
    this->somode = false;
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
  return pixel;
}

// int LineLayout::redrawLineRegion(const RowCol &root, LineLayout *layout, Line
// *l,
//                              int i, int bpos, int epos) {
//   int j, pos, rcol, ncol, delta = 1;
//   int column = layout->currentColumn;
//   int bcol, ecol;
//
//   if (l == NULL)
//     return 0;
//   pos = l->columnPos(column);
//   auto p = &(l->lineBuf[pos]);
//   auto pr = &(l->propBuf[pos]);
//   rcol = l->bytePosToColumn(pos);
//   bcol = bpos - pos;
//   ecol = epos - pos;
//
//   for (j = 0; rcol - column < layout->COLS && pos + j < l->len(); j += delta)
//   {
//     delta = get_mclen(&p[j]);
//     ncol = l->bytePosToColumn(pos + j + delta);
//     if (ncol - column > layout->COLS)
//       break;
//     if (j >= bcol && j < ecol) {
//       if (rcol < column) {
//         RowCol pixel{.row = i, .col = root.col};
//         for (rcol = column; rcol < ncol; rcol++) {
//           this->addch(pixel, ' ');
//           ++pixel.col;
//         }
//         continue;
//       }
//       RowCol pixel{.row = i, .col = rcol - column + root.col};
//       if (p[j] == '\t') {
//         for (; rcol < ncol; rcol++) {
//           this->addch(pixel, ' ');
//           ++pixel.col;
//         }
//       } else
//         pixel = this->addnstr(pixel, &p[j], delta);
//     }
//     rcol = ncol;
//   }
//   if (somode) {
//     somode = false;
//     this->standend();
//   }
//   if (ulmode) {
//     ulmode = false;
//     this->underlineend();
//   }
//   if (bomode) {
//     bomode = false;
//     this->boldend();
//   }
//   if (emph_mode) {
//     emph_mode = false;
//     this->boldend();
//   }
//
//   if (anch_mode) {
//     anch_mode = false;
//     this->underlineend();
//   }
//   if (imag_mode) {
//     imag_mode = false;
//     this->standend();
//   }
//   if (form_mode) {
//     form_mode = false;
//     this->standend();
//   }
//   if (visited_mode) {
//     visited_mode = false;
//   }
//   if (active_mode) {
//     active_mode = false;
//     this->boldend();
//   }
//   if (mark_mode) {
//     mark_mode = false;
//     this->standend();
//   }
//   if (graph_mode) {
//     graph_mode = false;
//     this->graphend();
//   }
//   return rcol - column;
// }
