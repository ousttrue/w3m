#pragma once
#include "rowcol.h"

// LineData
// +--->Width
// | ScrollView
// |  row
// |col+---->width
// |   |
// |   v
// |  height
// |
// | Cursor
// |  row
// |col+
// v
// Height
struct CursorAndScroll {
  // equals LineData lines.size() & max.col
  RowCol size = {0, 0};
  // cursor in Size rect.
  RowCol cursor = {0, 0};
  // scroll origin in Size rect.
  RowCol scroll = {0, 0};
  // equals ftxui::Node::box_
  RowCol view = {0, 0};

  // カーソルをビューに 合わせる
  void _cursor_to_view() {}

  // ビューをカーソルに合わせる
  void _view_to_cursor() {}

  void cursorHome() {
    // this->visualpos = 0;
    this->scroll = {0, 0};
    this->cursor = {0, 0};
  }

  void cursorCol(int col) {
    cursor.col = col;
    _view_to_cursor();
  }

  void cursorMoveCol(int col) {
    cursor.col += col;
    _view_to_cursor();
  }

  void cursorRow(int row) {
    cursor.row = row;
    _view_to_cursor();
  }

  void cursorMoveRow(int row) {
    cursor.row += row;
    _view_to_cursor();
  }

  void restorePosition(const CursorAndScroll &orig) {
    this->cursor = orig.cursor;
    this->scroll = orig.scroll;
    _view_to_cursor();
  }

  void scrollMoveRow(int row) {
    scroll.row += row;
    _cursor_to_view();
  }

  void scrollMoveCol(int col) {
    scroll.col += col;
    _cursor_to_view();
  }
};
