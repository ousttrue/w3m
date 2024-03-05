#include "anchor.h"
#include "readbuffer.h"
#include "line_layout.h"
#include "Str.h"
#include "quote.h"
#include "entity.h"
#include "http_request.h"
#include "myctype.h"
#include "regex.h"
#include "alloc.h"

bool MarkAllPages = false;

#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */

#define FIRST_ANCHOR_SIZE 30

int Anchor::onAnchor(int line, int pos) {
  BufferPoint bp;
  bp.line = line;
  bp.pos = pos;
  if (bp.cmp(this->start) < 0) {
    return -1;
  }
  if (this->end.cmp(bp) <= 0) {
    return 1;
  }
  return 0;
}

#define FIRST_MARKER_SIZE 30
void HmarkerList::putHmarker(int line, int pos, int seq) {
  // if (seq + 1 > ml->nmark)
  //   ml->nmark = seq + 1;
  // if (ml->nmark >= ml->markmax) {
  //   ml->markmax = ml->nmark * 2;
  //   ml->marks = (BufferPoint *)New_Reuse(BufferPoint, ml->marks,
  //   ml->markmax);
  // }
  while (seq >= (int)marks.size()) {
    marks.push_back({});
  }
  this->marks[seq].line = line;
  this->marks[seq].pos = pos;
  this->marks[seq].invalid = 0;
}

const char *html_quote(const char *str) {
  Str *tmp = NULL;
  for (auto p = str; *p; p++) {
    auto q = html_quote_char(*p);
    if (q) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_charp(tmp, q);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return Strnew_charp(str)->ptr;
}

const char *html_unquote(const char *str) {
  Str *tmp = NULL;
  const char *p, *q;

  for (p = str; *p;) {
    if (*p == '&') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      q = getescapecmd(&p);
      Strcat_charp(tmp, q);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
      p++;
    }
  }

  if (tmp)
    return tmp->ptr;
  return str;
}

const char *getescapecmd(const char **s) {
  const char *save = *s;
  Str *tmp;
  int ch = getescapechar(s);

  if (ch >= 0)
    return conv_entity(ch);

  if (*save != '&')
    tmp = Strnew_charp("&");
  else
    tmp = Strnew();
  Strcat_charp_n(tmp, save, *s - save);
  return tmp->ptr;
}

int getescapechar(const char **str) {
  int dummy = -1;
  const char *p = *str, *q;
  int strict_entity = true;

  if (*p == '&')
    p++;
  if (*p == '#') {
    p++;
    if (*p == 'x' || *p == 'X') {
      p++;
      if (!IS_XDIGIT(*p)) {
        *str = p;
        return -1;
      }
      for (dummy = GET_MYCDIGIT(*p), p++; IS_XDIGIT(*p); p++)
        dummy = dummy * 0x10 + GET_MYCDIGIT(*p);
      if (*p == ';')
        p++;
      *str = p;
      return dummy;
    } else {
      if (!IS_DIGIT(*p)) {
        *str = p;
        return -1;
      }
      for (dummy = GET_MYCDIGIT(*p), p++; IS_DIGIT(*p); p++)
        dummy = dummy * 10 + GET_MYCDIGIT(*p);
      if (*p == ';')
        p++;
      *str = p;
      return dummy;
    }
  }
  if (!IS_ALPHA(*p)) {
    *str = p;
    return -1;
  }
  q = p;
  for (p++; IS_ALNUM(*p); p++)
    ;
  q = allocStr(q, p - q);
  if (strcasestr("lt gt amp quot apos nbsp", q) && *p != '=') {
    /* a character entity MUST be terminated with ";". However,
     * there's MANY web pages which uses &lt , &gt or something
     * like them as &lt;, &gt;, etc. Therefore, we treat the most
     * popular character entities (including &#xxxx;) without
     * the last ";" as character entities. If the trailing character
     * is "=", it must be a part of query in an URL. So &lt=, &gt=, etc.
     * are not regarded as character entities.
     */
    strict_entity = false;
  }
  if (*p == ';')
    p++;
  else if (strict_entity) {
    *str = p;
    return -1;
  }
  *str = p;
  return getHash_si(&entity, q, -1);
}
