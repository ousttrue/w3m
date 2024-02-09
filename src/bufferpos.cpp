#include "bufferpos.h"
#include "buffer.h"
#include "display.h"
#include "alloc.h"
#include "tabbuffer.h"

void save_buffer_position(Buffer *buf) {
  BufferPos *b = buf->undo;

  if (!buf->layout.firstLine)
    return;
  if (b && b->top_linenumber == buf->layout.TOP_LINENUMBER() &&
      b->cur_linenumber == buf->layout.CUR_LINENUMBER() &&
      b->currentColumn == buf->layout.currentColumn && b->pos == buf->pos)
    return;
  b = (BufferPos *)New(BufferPos);
  b->top_linenumber = buf->layout.TOP_LINENUMBER();
  b->cur_linenumber = buf->layout.CUR_LINENUMBER();
  b->currentColumn = buf->layout.currentColumn;
  b->pos = buf->pos;
  b->bpos = buf->layout.currentLine ? buf->layout.currentLine->bpos : 0;
  b->next = NULL;
  b->prev = buf->undo;
  if (buf->undo)
    buf->undo->next = b;
  buf->undo = b;
}

void resetPos(BufferPos *b) {
  auto buf = new Buffer(0);
  auto top = new Line(b->top_linenumber);
  buf->layout.topLine = top;

  auto cur = new Line(b->cur_linenumber);
  cur->bpos = b->bpos;

  buf->layout.currentLine = cur;
  buf->pos = b->pos;
  buf->layout.currentColumn = b->currentColumn;
  restorePosition(Currentbuf, buf);
  Currentbuf->undo = b;
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}
