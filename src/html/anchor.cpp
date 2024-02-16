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
int PagerMax = PAGER_MAX_LINE;

#define FIRST_ANCHOR_SIZE 30

int Anchor::onAnchor(int line, int pos) {
  BufferPoint bp;
  bp.line = line;
  bp.pos = pos;
  if (bpcmp(bp, this->start) < 0) {
    return -1;
  }
  if (bpcmp(this->end, bp) <= 0) {
    return 1;
  }
  return 0;
}

Anchor *AnchorList::retrieveAnchor(int line, int pos) {

  if (this->size() == 0)
    return NULL;

  if (this->acache < 0 || static_cast<size_t>(this->acache) >= this->size())
    this->acache = 0;

  for (int b = 0, e = this->size() - 1; b <= e; this->acache = (b + e) / 2) {
    auto a = &this->anchors[this->acache];
    auto cmp = a->onAnchor(line, pos);
    if (cmp == 0)
      return a;
    else if (cmp > 0)
      b = this->acache + 1;
    else if (this->acache == 0)
      return NULL;
    else
      e = this->acache - 1;
  }
  return NULL;
}

Anchor *AnchorList::searchAnchor(std::string_view str) {
  for (size_t i = 0; i < this->size(); i++) {
    auto a = &this->anchors[i];
    if (a->hseq < 0) {
      continue;
    }
    if (a->url == str) {
      return a;
    }
  }
  return {};
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

Anchor *AnchorList::closest_next_anchor(Anchor *an, int x, int y) {
  if (this->size() == 0)
    return an;
  for (size_t i = 0; i < this->size(); i++) {
    if (this->anchors[i].hseq < 0)
      continue;
    if (this->anchors[i].start.line > y ||
        (this->anchors[i].start.line == y && this->anchors[i].start.pos > x)) {
      if (an == NULL || an->start.line > this->anchors[i].start.line ||
          (an->start.line == this->anchors[i].start.line &&
           an->start.pos > this->anchors[i].start.pos))
        an = &this->anchors[i];
    }
  }
  return an;
}

Anchor *AnchorList ::closest_prev_anchor(Anchor *an, int x, int y) {
  if (this->size() == 0)
    return an;
  for (size_t i = 0; i < this->size(); i++) {
    if (this->anchors[i].hseq < 0)
      continue;
    if (this->anchors[i].end.line < y ||
        (this->anchors[i].end.line == y && this->anchors[i].end.pos <= x)) {
      if (an == NULL || an->end.line < this->anchors[i].end.line ||
          (an->end.line == this->anchors[i].end.line &&
           an->end.pos < this->anchors[i].end.pos))
        an = &this->anchors[i];
    }
  }
  return an;
}

void AnchorList::shiftAnchorPosition(HmarkerList *hl, int line, int pos,
                                     int shift) {
  if (this->size() == 0)
    return;

  auto s = this->size() / 2;
  size_t b, e;
  for (b = 0, e = this->size() - 1; b <= e; s = (b + e + 1) / 2) {
    auto a = &this->anchors[s];
    auto cmp = a->onAnchor(line, pos);
    if (cmp == 0)
      break;
    else if (cmp > 0)
      b = s + 1;
    else if (s == 0)
      break;
    else
      e = s - 1;
  }
  for (; s < this->size(); s++) {
    auto a = &this->anchors[s];
    if (a->start.line > line)
      break;
    if (a->start.pos > pos) {
      a->start.pos += shift;
      if (hl->size() && a->hseq >= 0 && hl->marks[a->hseq].line == line)
        hl->marks[a->hseq].pos = a->start.pos;
    }
    if (a->end.pos >= pos)
      a->end.pos += shift;
  }
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

void AnchorList::reseq_anchor0(short *seqmap) {
  for (size_t i = 0; i < this->size(); i++) {
    auto a = &this->anchors[i];
    if (a->hseq >= 0) {
      a->hseq = seqmap[a->hseq];
    }
  }
}

Anchor *AnchorList::putAnchor(const char *url, const char *target,
                              const HttpOption &option, const char *title,
                              unsigned char key, int line, int pos) {
  BufferPoint bp = {0};
  bp.line = line;
  bp.pos = pos;

  size_t n = this->size();
  size_t i;
  if (!n || bpcmp(this->anchors[n - 1].start, bp) < 0) {
    i = n;
  } else {
    for (i = 0; i < n; i++) {
      if (bpcmp(this->anchors[i].start, bp) >= 0) {
        while (n >= this->anchors.size()) {
          this->anchors.push_back({});
        }
        for (size_t j = n; j > i; j--)
          this->anchors[j] = this->anchors[j - 1];
        break;
      }
    }
  }

  while (i >= this->anchors.size()) {
    this->anchors.push_back({});
  }
  auto a = &this->anchors[i];
  a->url = url;
  a->target = target;
  a->option = option;
  a->title = title;
  a->accesskey = key;
  a->slave = false;
  a->start = bp;
  a->end = bp;
  return a;
}
