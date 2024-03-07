#include "screen.h"
#include "line_layout.h"
#include <ftxui/screen/screen.hpp>

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

RowCol Screen::addnstr(RowCol pos, const char *s, int n, ScreenFlags mode) {
  for (int i = 0; i < n && *s != '\0'; i++) {
    auto utf8 = Utf8::from((const char8_t *)s);
    addmch(pos, utf8, mode);
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

void Screen::toggle_stand(const RowCol &pos) {
  auto &pixel = this->pixel(pos);
  pixel.inverted = !pixel.inverted;
}

void Screen::addmch(const RowCol &pos, const Utf8 &utf8, ScreenFlags mode) {
  auto &pixel = this->pixel(pos);
  pixel.character = utf8.view();
  pixel.bold = (int)(mode & ScreenFlags::BOLD);
  pixel.underlined = (int)(mode & ScreenFlags::UNDERLINE);
}

std::string Screen::str(const RowCol &root, LineLayout *layout) {
  // draw lines
  if (cline != layout->topLine || ccolumn != layout->currentColumn) {
    int i = 0;
    for (auto l = layout->topLine; i < layout->LINES && l; i++, ++l) {
      auto pixel = this->redrawLine(
          {
              .row = i + root.row,
              .col = root.col,
          },
          l, layout->COLS, layout->currentColumn);
      this->clrtoeolx(pixel);
    }
    // clear remain
    this->clrtobotx({
        .row = i + root.row,
        .col = 0,
    });
    cline = layout->topLine;
    ccolumn = layout->currentColumn;
  }
  assert(layout->topLine);

  // highlight active anchor
  // if (!layout->empty() && layout->data._hmarklist) {
  //   if (layout->data._href || layout->data._formitem) {
  //     class ActiveAnchorDrawer d(this, root, layout);
  //     d.drawAnchorCursor();
  //   }
  // }

  // toStr
  return this->_screen->ToString();
}

std::shared_ptr<ftxui::Screen> Screen::render(const ftxui::Box &box,
                                              LineLayout *layout) {

  _screen = std::make_shared<ftxui::Screen>(box.x_max - box.x_min + 1,
                                            box.y_max - box.y_min + 1);
  auto l = layout->data.firstLine();
  int i = 0;
  for (; i < layout->data.lines.size(); ++i, ++l) {
    auto pixel = this->redrawLine(
        {
            .row = i,
            .col = 0,
        },
        l, layout->COLS, layout->currentColumn);
    this->clrtoeolx(pixel);
  }
  // clear remain
  this->clrtobotx({
      .row = i,
      .col = 0,
  });
  cline = layout->topLine;
  ccolumn = layout->currentColumn;

  return _screen;
}

static ScreenFlags propToFlag(Lineprop prop) {
  ScreenFlags flag =
      (prop & PE_BOLD ? ScreenFlags::BOLD : ScreenFlags::NORMAL) |
      (prop & PE_UNDER ? ScreenFlags::UNDERLINE : ScreenFlags::NORMAL) |
      (prop & PE_STAND ? ScreenFlags::STANDOUT : ScreenFlags::NORMAL);
  return flag;
}

RowCol Screen::redrawLine(RowCol pixel, Line *l, int cols, int scrollLeft) {
  if (l->len() == 0 || l->width() - 1 < pixel.col) {
    return pixel;
  }

  int pos = l->columnPos(pixel.col);
  auto p = &(l->lineBuf[pos]);
  auto pr = &(l->propBuf[pos]);
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

// int Screen::redrawLineRegion(const RowCol &root, LineLayout *layout, Line *l,
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
