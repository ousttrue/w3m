#pragma once
#include <ostream>

struct RowCol {
  int row = 0;
  int col = 0;

  RowCol operator+(const RowCol &rhs) const {
    return {
        .row = row + rhs.row,
        .col = col + rhs.col,
    };
  }

  RowCol operator-(const RowCol &rhs) const {
    return {
        .row = row - rhs.row,
        .col = col - rhs.col,
    };
  }
};

inline std::ostream &operator<<(std::ostream &os, const RowCol &rc) {
  os << "{row:" << rc.row << ",col:" << rc.col << "}";
  return os;
}
