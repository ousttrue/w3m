#pragma once

struct TermSize {
  int lines;
  int cols;
};

struct TermSize term_size();
struct TermSize term_setlinescols();
