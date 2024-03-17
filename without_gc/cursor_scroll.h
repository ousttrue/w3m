#pragma once
#include "rowcol.h"
#include <ostream>

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
class CursorAndScroll {
  friend std::ostream &operator<<(std::ostream &os, const CursorAndScroll &cs);

  // equals LineData lines.size() & max.col
  RowCol _size = {0, 0};
  // cursor in Size rect.
  RowCol _cursor = {0, 0};
  // scroll origin in Size rect.
  RowCol _scroll = {0, 0};
  // equals ftxui::Node::box_
  RowCol _view = {0, 0};

  // カーソルをビューに 合わせる
  void _cursor_to_view() {
    // row
    if (_cursor.row < 0) {
      _cursor.row = 0;
    }
    if (_scroll.row < 0) {
      _scroll.row = 0;
    }
    if (_cursor.row < _scroll.row) {
      _cursor.row = _scroll.row;
    } else if (_cursor.row >= _scroll.row + _view.row) {
      _cursor.row = _scroll.row + _view.row - 1;
    }
    // col
    if (_cursor.col < 0) {
      _cursor.col = 0;
    }
    if (_scroll.col < 0) {
      _scroll.col = 0;
    }
    if (_cursor.col < _scroll.col) {
      _cursor.col = _scroll.col;
    } else if (_cursor.col >= _scroll.col + _view.col) {
      _cursor.col = _scroll.col + _view.col - 1;
    }
  }

  // ビューをカーソルに合わせる
  void _view_to_cursor() {
    // row
    if (_cursor.row < 0) {
      _cursor.row = 0;
    }
    if (_scroll.row < 0) {
      _scroll.row = 0;
    }
    if (_cursor.row < _scroll.row) {
      _scroll.row = _cursor.row;
    } else if (_cursor.row >= _scroll.row + _view.row) {
      _scroll.row = _cursor.row - _view.row + 1;
    }
    // col
    if (_cursor.col < 0) {
      _cursor.col = 0;
    }
    if (_scroll.col < 0) {
      _scroll.col = 0;
    }
    if (_cursor.col < _scroll.col) {
      _scroll.col = _cursor.col;
    } else if (_cursor.col >= _scroll.col + _view.col) {
      _scroll.col = _cursor.col - _view.col + 1;
    }
  }

public:
  void size(const RowCol &size) { _size = size; }

  const RowCol &cursor() const { return _cursor; }

  void cursorCol(int col) {
    _cursor.col = col;
    _view_to_cursor();
  }

  void cursorMoveCol(int col) {
    _cursor.col += col;
    _view_to_cursor();
  }

  void cursorRow(int row) {
    _cursor.row = row;
    _view_to_cursor();
  }

  void cursorMoveRow(int row) {
    _cursor.row += row;
    _view_to_cursor();
  }

  const RowCol &scroll() const { return _scroll; }

  void scrollMoveRow(int row) {
    _scroll.row += row;
    _cursor_to_view();
  }

  void scrollMoveCol(int col) {
    _scroll.col += col;
    _cursor_to_view();
  }

  void cursorHome() {
    // this->visualpos = 0;
    this->_scroll = {0, 0};
    this->_cursor = {0, 0};
  }

  void restorePosition(const CursorAndScroll &orig) {
    _cursor = orig._cursor;
    _scroll = orig._scroll;
    _view_to_cursor();
  }

  void view(const RowCol &view) { _view = view; }
};

inline std::ostream &operator<<(std::ostream &os, const CursorAndScroll &cs) {
  os << "[cursor]" << cs._cursor << "[scroll]" << cs._scroll << "[view]"
     << cs._view << "[size]" << cs._size;
  // ss << "[cursor]" << buf->layout->visual.cursor().row << ","
  //    << buf->layout->visual.cursor().col
  //    //
  //    << "(pos)"
  //    << buf->layout->cursorPos()
  //    //
  //    << "[scroll]" << buf->layout->visual.scroll().row << ","
  //    << buf->layout->visual.scroll().col
  //     //
  //     ;
  return os;
}
