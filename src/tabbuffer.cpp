#include "tabbuffer.h"
#include "buffer.h"
#include "utf8.h"
#include "screen.h"
#include <sys/stat.h>

bool open_tab_blank = false;
bool open_tab_dl_list = false;
bool close_tab_back = false;
int TabCols = 10;

TabBuffer::TabBuffer() {}

TabBuffer::~TabBuffer() {}

int TabBuffer::draw(Screen *screen, TabBuffer *current) {
  if (this == current) {
    screen->bold();
  } else {
    screen->boldend();
  }
  RowCol pos{.row = this->y, .col = this->x1};
  screen->addch(pos, '[');
  auto l = this->x2 - this->x1 - 1 -
           get_strwidth(this->currentBuffer()->layout.data.title.c_str());
  if (l < 0) {
    l = 0;
  }
  if (l / 2 > 0) {
    pos = screen->addnstr_sup(pos, " ", l / 2);
  }

  if (this == current) {
    screen->underline();
    // standout();
  }
  screen->addnstr(pos, this->currentBuffer()->layout.data.title.c_str(),
                  this->x2 - this->x1 - l);
  if (this == current) {
    screen->underlineend();
    // standend();
  }

  if ((l + 1) / 2 > 0) {
    pos = screen->addnstr_sup(pos, " ", (l + 1) / 2);
  }
  screen->addch({.row = this->y, .col = this->x2}, ']');
  if (this == current) {
    screen->boldend();
  }
  return this->y;
}

/*
 * deleteBuffer: delete buffer
 */
void TabBuffer::deleteBuffer(const std::shared_ptr<Buffer> &delbuf) {
  if (!delbuf) {
    return;
  }

  if (_currentBuffer == delbuf) {
    _currentBuffer = delbuf->backBuffer;
  }

  if (firstBuffer == delbuf && firstBuffer->backBuffer != nullptr) {
    firstBuffer = firstBuffer->backBuffer;
  } else if (auto buf = forwardBuffer(delbuf)) {
  }

  if (!this->currentBuffer()) {
    _currentBuffer = this->firstBuffer;
  }
}

void TabBuffer::pushBuffer(const std::shared_ptr<Buffer> &buf) {
  // pushHashHist(URLHist,
  // this->currentBuffer()->res->currentURL.to_Str().c_str());
  if (this->firstBuffer == this->currentBuffer()) {
    buf->backBuffer = this->firstBuffer;
    this->firstBuffer = buf;
  } else if (auto b = forwardBuffer(this->currentBuffer())) {
    buf->backBuffer = this->currentBuffer();
    b->backBuffer = buf;
  }
  _currentBuffer = buf;
  this->currentBuffer()->saveBufferInfo();
}

/*
 * namedBuffer: Select buffer which have specified name
 */
std::shared_ptr<Buffer> namedBuffer(const std::shared_ptr<Buffer> &first,
                                    char *name) {
  if (first->layout.data.title == name) {
    return first;
  }
  for (auto buf = first; buf->backBuffer != nullptr; buf = buf->backBuffer) {
    if (buf->backBuffer->layout.data.title == name) {
      return buf->backBuffer;
    }
  }
  return nullptr;
}

void TabBuffer::repBuffer(const std::shared_ptr<Buffer> &oldbuf,
                          const std::shared_ptr<Buffer> &buf) {
  this->firstBuffer = replaceBuffer(oldbuf, buf);
  _currentBuffer = buf;
}

// bool TabBuffer::select(char cmd, const std::shared_ptr<Buffer> &buf) {
//   switch (cmd) {
//   case 'B':
//     return true;
//
//   case '\n':
//   case ' ':
//     _currentBuffer = buf;
//     return true;
//
//   case 'D':
//     this->deleteBuffer(buf);
//     if (this->firstBuffer == nullptr) {
//       /* No more buffer */
//       this->firstBuffer = Buffer::nullBuffer();
//       _currentBuffer = this->firstBuffer;
//     }
//     break;
//
//   case 'q':
//     qquitfm();
//     break;
//
//   case 'Q':
//     quitfm();
//     break;
//   }
//
//   return false;
// }

/*
 * replaceBuffer: replace buffer
 */
std::shared_ptr<Buffer>
TabBuffer::replaceBuffer(const std::shared_ptr<Buffer> &delbuf,
                         const std::shared_ptr<Buffer> &newbuf) {

  auto first = this->firstBuffer;
  if (delbuf == nullptr) {
    newbuf->backBuffer = first;
    return newbuf;
  }

  if (first == delbuf) {
    newbuf->backBuffer = delbuf->backBuffer;
    return newbuf;
  }

  std::shared_ptr<Buffer> buf;
  if (delbuf && (buf = forwardBuffer(delbuf))) {
    buf->backBuffer = newbuf;
    newbuf->backBuffer = delbuf->backBuffer;
    return first;
  }
  newbuf->backBuffer = first;
  return newbuf;
}

std::shared_ptr<Buffer>
TabBuffer::forwardBuffer(const std::shared_ptr<Buffer> &buf) {
  std::shared_ptr<Buffer> b;
  for (b = firstBuffer; b != nullptr && b->backBuffer != buf; b = b->backBuffer)
    ;
  return b;
}
