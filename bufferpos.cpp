#include "bufferpos.h"
#include "buffer.h"
#include "display.h"
#include "alloc.h"

void save_buffer_position(Buffer *buf) {
  BufferPos *b = buf->undo;

  if (!buf->firstLine)
    return;
  if (b && b->top_linenumber == TOP_LINENUMBER(buf) &&
      b->cur_linenumber == CUR_LINENUMBER(buf) &&
      b->currentColumn == buf->currentColumn && b->pos == buf->pos)
    return;
  b = (BufferPos *)New(BufferPos);
  b->top_linenumber = TOP_LINENUMBER(buf);
  b->cur_linenumber = CUR_LINENUMBER(buf);
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
  auto buf = new Buffer(0);
  auto top = new Line(b->top_linenumber);
  buf->topLine = top;

  auto cur = new Line(b->cur_linenumber);
  cur->bpos = b->bpos;

  buf->currentLine = cur;
  buf->pos = b->pos;
  buf->currentColumn = b->currentColumn;
  restorePosition(Currentbuf, buf);
  Currentbuf->undo = b;
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}
