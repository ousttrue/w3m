#pragma once

struct RowCol {
  int row = 0;
  int col = 0;

  RowCol operator+(const RowCol &rhs) const {
    return {
        .row = row + rhs.row,
        .col = col + rhs.col,
    };
  }
};

struct Viewport {
  RowCol root = {};
  RowCol size = {};
};
