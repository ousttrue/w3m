#include "html_feed_env.h"
#include "textlist.h"
#include "readbuffer.h"

html_feed_environ::html_feed_environ(int nenv, TextLineList *buf, int limit,
                                     int indent)
    : buf(buf) {
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

  if (!(tl = rpopTextLine(this->buf)))
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
