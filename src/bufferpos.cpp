#include "bufferpos.h"
#include "app.h"
#include "display.h"
#include "alloc.h"
#include "tabbuffer.h"
#include "line_layout.h"
#include "buffer.h"

void save_buffer_position(LineLayout *buf) {
  BufferPos *b = buf->undo;

  if (!buf->firstLine)
    return;
  if (b && b->top_linenumber == buf->TOP_LINENUMBER() &&
      b->cur_linenumber == buf->CUR_LINENUMBER() &&
      b->currentColumn == buf->currentColumn && b->pos == buf->pos)
    return;
  b = (BufferPos *)New(BufferPos);
  b->top_linenumber = buf->TOP_LINENUMBER();
  b->cur_linenumber = buf->CUR_LINENUMBER();
  b->currentColumn = buf->currentColumn;
  b->pos = buf->pos;
  b->bpos = buf->currentLine ? buf->currentLine->bpos : 0;
  b->next = NULL;
  b->prev = buf->undo;
  if (buf->undo)
    buf->undo->next = b;
  buf->undo = b;
}

void resetPos(BufferPos *b) {
  LineLayout buf;
  auto top = new Line(b->top_linenumber);
  buf.topLine = top;

  auto cur = new Line(b->cur_linenumber);
  cur->bpos = b->bpos;

  buf.currentLine = cur;
  buf.pos = b->pos;
  buf.currentColumn = b->currentColumn;
  CurrentTab->currentBuffer()->layout.restorePosition(buf);
  CurrentTab->currentBuffer()->layout.undo = b;
  displayBuffer();
}
