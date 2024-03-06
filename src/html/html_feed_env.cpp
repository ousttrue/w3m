#include "html_feed_env.h"
#include "readbuffer.h"
#include "textline.h"
#include "Str.h"

html_feed_environ::html_feed_environ(int nenv, int limit, int indent,
                                     GeneralList<TextLine> *_buf)
    : buf(_buf) {
  assert(nenv);
  envs.resize(nenv);
  this->tagbuf = Strnew();
  this->limit = limit;
  envs[0].indent = indent;
}

int sloppy_parse_line(char **str) {
  if (**str == '<') {
    while (**str && **str != '>')
      (*str)++;
    if (**str == '>')
      (*str)++;
    return 1;
  } else {
    while (**str && **str != '<')
      (*str)++;
    return 0;
  }
}

void html_feed_environ::purgeline() {
  char *p, *q;
  Str *tmp;
  TextLine *tl;

  if (this->buf == NULL || this->blank_lines == 0)
    return;

  if (!(tl = this->buf->rpopValue()))
    return;
  p = tl->line->ptr;
  tmp = Strnew();
  while (*p) {
    q = p;
    if (sloppy_parse_line(&p)) {
      Strcat_charp_n(tmp, q, p - q);
    }
  }
  appendTextLine(this->buf, tmp, 0);
  this->blank_lines--;
}
