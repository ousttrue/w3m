#pragma once

extern bool nextpage_topline;

struct BufferPos {
  long top_linenumber;
  long cur_linenumber;
  int currentColumn;
  int pos;
  int bpos;
  struct BufferPos *next;
  struct BufferPos *prev;
};

struct Viewport {
  short COLS;
  short LINES;
  int currentColumn;
  short cursorX;
  short cursorY;
  int pos;
  int visualpos;
  short rootX;
  short rootY;
  struct BufferPos *undo;
};

struct Line *lineSkip(struct Viewport *doc, struct Line *line,
                      struct Line *lastLine, int offset, int last);
struct Line *render_line(struct Line *l, const struct Viewport *viewport,
                         int row);
void render_line_region(struct Viewport *viewport, struct Line *l, int i,
                        int bpos, int epos);
