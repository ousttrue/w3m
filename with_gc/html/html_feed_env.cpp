#include "html_feed_env.h"
#include "readbuffer.h"
#include "textline.h"
#include "stringtoken.h"
#include <assert.h>

html_feed_environ::html_feed_environ(int nenv, int limit, int indent,
                                     GeneralList<TextLine> *_buf)
    : buf(_buf) {
  assert(nenv);
  envs.resize(nenv);
  this->tagbuf = Strnew();
  this->limit = limit;
  envs[0].indent = indent;
}

void html_feed_environ::purgeline() {
  if (this->buf == NULL || this->obuf.blank_lines == 0)
    return;

  TextLine *tl;
  if (!(tl = this->buf->rpopValue()))
    return;

  const char *p = tl->line->ptr;
  auto tmp = Strnew();
  while (*p) {
    stringtoken st(p);
    auto token = st.sloppy_parse_line();
    p = st.ptr();
    if (token) {
      Strcat(tmp, *token);
    }
  }
  appendTextLine(this->buf, tmp, 0);
  this->obuf.blank_lines--;
}
