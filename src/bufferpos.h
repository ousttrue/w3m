#pragma once

struct BufferPos {
  long top_linenumber;
  long cur_linenumber;
  int currentColumn;
  int pos;
  int bpos;
  BufferPos *next;
  BufferPos *prev;
};

struct LineLayout;
void save_buffer_position(LineLayout *buf);
void resetPos(BufferPos *b);
