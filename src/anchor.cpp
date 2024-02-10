#include "anchor.h"
#include "loadproc.h"
#include "url_quote.h"
#include "w3m.h"
#include "url_stream.h"
#include "quote.h"
#include "entity.h"
#include "readbuffer.h"
#include "rc.h"
#include "terms.h"
#include "linklist.h"
#include "http_request.h"
#include "line.h"
#include "buffer.h"
#include "form.h"
#include "myctype.h"
#include "regex.h"
#include "proto.h"
#include "alloc.h"

bool MarkAllPages = false;

#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */
int PagerMax = PAGER_MAX_LINE;

#define FIRST_ANCHOR_SIZE 30

int onAnchor(Anchor *a, int line, int pos) {
  BufferPoint bp;
  bp.line = line;
  bp.pos = pos;

  if (bpcmp(bp, a->start) < 0)
    return -1;
  if (bpcmp(a->end, bp) <= 0)
    return 1;
  return 0;
}

Anchor *AnchorList::retrieveAnchor(int line, int pos) {

  if (this->size() == 0)
    return NULL;

  if (this->acache < 0 || static_cast<size_t>(this->acache) >= this->size())
    this->acache = 0;

  Anchor *a;
  size_t b, e;
  int cmp;
  for (b = 0, e = this->size() - 1; b <= e; this->acache = (b + e) / 2) {
    a = &this->anchors[this->acache];
    cmp = onAnchor(a, line, pos);
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

Anchor *retrieveCurrentAnchor(Buffer *buf) {
  if (!buf->layout.currentLine || !buf->layout.href)
    return NULL;
  return buf->layout.href->retrieveAnchor(buf->layout.currentLine->linenumber,
                                          buf->layout.pos);
}

Anchor *retrieveCurrentImg(Buffer *buf) {
  if (!buf->layout.currentLine || !buf->layout.img)
    return NULL;
  return buf->layout.img->retrieveAnchor(buf->layout.currentLine->linenumber,
                                         buf->layout.pos);
}

Anchor *retrieveCurrentForm(Buffer *buf) {
  if (!buf->layout.currentLine || !buf->layout.formitem)
    return NULL;
  return buf->layout.formitem->retrieveAnchor(
      buf->layout.currentLine->linenumber, buf->layout.pos);
}

Anchor *searchAnchor(AnchorList *al, const char *str) {
  Anchor *a;
  if (al == NULL)
    return NULL;
  for (size_t i = 0; i < al->size(); i++) {
    a = &al->anchors[i];
    if (a->hseq < 0)
      continue;
    if (!strcmp(a->url, str))
      return a;
  }
  return NULL;
}

static Anchor *_put_anchor_all(Buffer *buf, const char *p1, const char *p2,
                               int line, int pos) {
  Str *tmp;

  tmp = Strnew_charp_n(p1, p2 - p1);
  return buf->layout.registerHref(url_quote(tmp->ptr).c_str(), NULL, NO_REFERER,
                                  NULL, '\0', line, pos);
}

static void reseq_anchor0(AnchorList *al, short *seqmap) {
  if (!al)
    return;

  for (size_t i = 0; i < al->size(); i++) {
    auto a = &al->anchors[i];
    if (a->hseq >= 0) {
      a->hseq = seqmap[a->hseq];
    }
  }
}

/* renumber anchor */
static void reseq_anchor(Buffer *buf) {
  if (!buf->layout.href)
    return;

  int nmark = (buf->layout.hmarklist) ? buf->layout.hmarklist->nmark : 0;
  int n = nmark;
  for (size_t i = 0; i < buf->layout.href->size(); i++) {
    auto a = &buf->layout.href->anchors[i];
    if (a->hseq == -2) {
      n++;
    }
  }
  if (n == nmark) {
    return;
  }

  auto seqmap = std::vector<short>(n);
  for (int i = 0; i < n; i++) {
    seqmap[i] = i;
  }

  n = nmark;
  HmarkerList *ml = NULL;
  for (size_t i = 0; i < buf->layout.href->size(); i++) {
    auto a = &buf->layout.href->anchors[i];
    if (a->hseq == -2) {
      a->hseq = n;
      auto a1 = closest_next_anchor(buf->layout.href, NULL, a->start.pos,
                                    a->start.line);
      a1 = closest_next_anchor(buf->layout.formitem, a1, a->start.pos,
                               a->start.line);
      if (a1 && a1->hseq >= 0) {
        seqmap[n] = seqmap[a1->hseq];
        for (int j = a1->hseq; j < nmark; j++) {
          seqmap[j]++;
        }
      }
      ml = putHmarker(ml, a->start.line, a->start.pos, seqmap[n]);
      n++;
    }
  }

  for (int i = 0; i < nmark; i++) {
    ml = putHmarker(ml, buf->layout.hmarklist->marks[i].line,
                    buf->layout.hmarklist->marks[i].pos, seqmap[i]);
  }
  buf->layout.hmarklist = ml;

  reseq_anchor0(buf->layout.href, seqmap.data());
  reseq_anchor0(buf->layout.formitem, seqmap.data());
}

static const char *reAnchorPos(Buffer *buf, Line *l, const char *p1,
                               const char *p2,
                               Anchor *(*anchorproc)(Buffer *, const char *,
                                                     const char *, int, int)) {
  Anchor *a;
  int spos, epos;
  int i;
  int hseq = -2;

  spos = p1 - l->lineBuf.data();
  epos = p2 - l->lineBuf.data();
  for (i = spos; i < epos; i++) {
    if (l->propBuf[i] & (PE_ANCHOR | PE_FORM))
      return p2;
  }
  for (i = spos; i < epos; i++)
    l->propBuf[i] |= PE_ANCHOR;
  while (spos > l->len && l->next && l->next->bpos) {
    spos -= l->len;
    epos -= l->len;
    l = l->next;
  }
  while (1) {
    a = anchorproc(buf, p1, p2, l->linenumber, spos);
    a->hseq = hseq;
    if (hseq == -2) {
      reseq_anchor(buf);
      hseq = a->hseq;
    }
    a->end.line = l->linenumber;
    if (epos > l->len && l->next && l->next->bpos) {
      a->end.pos = l->len;
      spos = 0;
      epos -= l->len;
      l = l->next;
    } else {
      a->end.pos = epos;
      break;
    }
  }
  return p2;
}

void reAnchorWord(Buffer *buf, Line *l, int spos, int epos) {
  reAnchorPos(buf, l, &l->lineBuf[spos], &l->lineBuf[epos], _put_anchor_all);
}

/* search regexp and register them as anchors */
/* returns error message if any               */
static const char *reAnchorAny(Buffer *buf, const char *re,
                               Anchor *(*anchorproc)(Buffer *, const char *,
                                                     const char *, int, int)) {
  Line *l;
  const char *p = NULL, *p1, *p2;

  if (re == NULL || *re == '\0') {
    return NULL;
  }
  if ((re = regexCompile(re, 1)) != NULL) {
    return re;
  }
  for (l = MarkAllPages ? buf->layout.firstLine : buf->layout.topLine;
       l != NULL &&
       (MarkAllPages ||
        l->linenumber < buf->layout.topLine->linenumber + LASTLINE);
       l = l->next) {
    if (p && l->bpos) {
      continue;
    }
    p = l->lineBuf.data();
    for (;;) {
      if (regexMatch(p, &l->lineBuf[l->size()] - p, p == l->lineBuf.data()) ==
          1) {
        matchedPosition(&p1, &p2);
        p = reAnchorPos(buf, l, p1, p2, anchorproc);
      } else
        break;
    }
  }
  return NULL;
}

const char *reAnchor(Buffer *buf, const char *re) {
  return reAnchorAny(buf, re, _put_anchor_all);
}

#define FIRST_MARKER_SIZE 30
HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq) {
  if (ml == NULL) {
    ml = (HmarkerList *)New(HmarkerList);
    ml->marks = NULL;
    ml->nmark = 0;
    ml->markmax = 0;
    ml->prevhseq = -1;
  }
  if (ml->markmax == 0) {
    ml->markmax = FIRST_MARKER_SIZE;
    ml->marks = (BufferPoint *)NewAtom_N(BufferPoint, ml->markmax);
    bzero(ml->marks, sizeof(BufferPoint) * ml->markmax);
  }
  if (seq + 1 > ml->nmark)
    ml->nmark = seq + 1;
  if (ml->nmark >= ml->markmax) {
    ml->markmax = ml->nmark * 2;
    ml->marks = (BufferPoint *)New_Reuse(BufferPoint, ml->marks, ml->markmax);
  }
  ml->marks[seq].line = line;
  ml->marks[seq].pos = pos;
  ml->marks[seq].invalid = 0;
  return ml;
}

Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y) {
  if (a == NULL || a->size() == 0)
    return an;
  for (size_t i = 0; i < a->size(); i++) {
    if (a->anchors[i].hseq < 0)
      continue;
    if (a->anchors[i].start.line > y ||
        (a->anchors[i].start.line == y && a->anchors[i].start.pos > x)) {
      if (an == NULL || an->start.line > a->anchors[i].start.line ||
          (an->start.line == a->anchors[i].start.line &&
           an->start.pos > a->anchors[i].start.pos))
        an = &a->anchors[i];
    }
  }
  return an;
}

Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y) {
  if (a == NULL || a->size() == 0)
    return an;
  for (size_t i = 0; i < a->size(); i++) {
    if (a->anchors[i].hseq < 0)
      continue;
    if (a->anchors[i].end.line < y ||
        (a->anchors[i].end.line == y && a->anchors[i].end.pos <= x)) {
      if (an == NULL || an->end.line < a->anchors[i].end.line ||
          (an->end.line == a->anchors[i].end.line &&
           an->end.pos < a->anchors[i].end.pos))
        an = &a->anchors[i];
    }
  }
  return an;
}

void shiftAnchorPosition(AnchorList *al, HmarkerList *hl, int line, int pos,
                         int shift) {
  if (al == NULL || al->size() == 0)
    return;

  auto s = al->size() / 2;
  size_t b, e;
  for (b = 0, e = al->size() - 1; b <= e; s = (b + e + 1) / 2) {
    auto a = &al->anchors[s];
    auto cmp = onAnchor(a, line, pos);
    if (cmp == 0)
      break;
    else if (cmp > 0)
      b = s + 1;
    else if (s == 0)
      break;
    else
      e = s - 1;
  }
  for (; s < al->size(); s++) {
    auto a = &al->anchors[s];
    if (a->start.line > line)
      break;
    if (a->start.pos > pos) {
      a->start.pos += shift;
      if (hl && hl->marks && a->hseq >= 0 && hl->marks[a->hseq].line == line)
        hl->marks[a->hseq].pos = a->start.pos;
    }
    if (a->end.pos >= pos)
      a->end.pos += shift;
  }
}

const char *getAnchorText(Buffer *buf, AnchorList *al, Anchor *a) {
  int hseq;
  Line *l;
  Str *tmp = NULL;
  char *p, *ep;

  if (!a || a->hseq < 0)
    return NULL;
  hseq = a->hseq;
  l = buf->layout.firstLine;
  for (size_t i = 0; i < al->size(); i++) {
    a = &al->anchors[i];
    if (a->hseq != hseq)
      continue;
    for (; l; l = l->next) {
      if (l->linenumber == a->start.line)
        break;
    }
    if (!l)
      break;
    p = l->lineBuf.data() + a->start.pos;
    ep = l->lineBuf.data() + a->end.pos;
    for (; p < ep && IS_SPACE(*p); p++)
      ;
    if (p == ep)
      continue;
    if (!tmp)
      tmp = Strnew_size(ep - p);
    else
      Strcat_char(tmp, ' ');
    Strcat_charp_n(tmp, p, ep - p);
  }
  return tmp ? tmp->ptr : NULL;
}

Buffer *link_list_panel(Buffer *buf) {
  LinkList *l;
  AnchorList *al;
  Anchor *a;
  FormItemList *fi;
  const char *u, *p;
  const char *t;
  Url pu;
  Str *tmp = Strnew_charp("<title>Link List</title>\
<h1 align=center>Link List</h1>\n");

  if (buf->layout.linklist) {
    Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
    for (l = buf->layout.linklist; l; l = l->next) {
      if (l->url) {
        pu = Url::parse(l->url, baseURL(buf));
        p = Strnew(pu.to_Str())->ptr;
        u = html_quote(p);
        if (DecodeURL)
          p = html_quote(url_decode0(p));
        else
          p = u;
      } else
        u = p = "";
      if (l->type == LINK_TYPE_REL)
        t = " [Rel]";
      else if (l->type == LINK_TYPE_REV)
        t = " [Rev]";
      else
        t = "";
      t = Sprintf("%s%s\n", l->title ? l->title : "", t)->ptr;
      t = html_quote(t);
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  if (buf->layout.href) {
    Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
    al = buf->layout.href;
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->hseq < 0 || a->slave)
        continue;
      pu = urlParse(a->url, baseURL(buf));
      p = Strnew(pu.to_Str())->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      t = getAnchorText(buf, al, a);
      t = t ? html_quote(t) : "";
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  if (buf->layout.img) {
    Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
    al = buf->layout.img;
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->slave)
        continue;
      pu = urlParse(a->url, baseURL(buf));
      p = Strnew(pu.to_Str())->ptr;
      u = html_quote(p);
      if (DecodeURL)
        p = html_quote(url_decode0(p));
      else
        p = u;
      if (a->title && *a->title)
        t = html_quote(a->title);
      else
        t = html_quote(url_decode0(a->url));
      Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p, "\n",
                     NULL);
      if (!buf->layout.formitem) {
        continue;
      }
      a = buf->layout.formitem->retrieveAnchor(a->start.line, a->start.pos);
      if (!a)
        continue;
      fi = (FormItemList *)a->url;
      fi = fi->parent->item;
      if (fi->parent->method == FORM_METHOD_INTERNAL &&
          !Strcmp_charp(fi->parent->action, "map") && fi->value) {
        // MapList *ml = searchMapList(buf, fi->value->ptr);
        // ListItem *mi;
        // MapArea *m;
        // if (!ml)
        //   continue;
        // Strcat_charp(tmp, "<br>\n<b>Image map</b>\n<ol>\n");
        // for (mi = ml->area->first; mi != NULL; mi = mi->next) {
        //   m = (MapArea *)mi->ptr;
        //   if (!m)
        //     continue;
        //   parseURL2(m->url, &pu, baseURL(buf));
        //   p = Url2Str(&pu)->ptr;
        //   u = html_quote(p);
        //   if (DecodeURL)
        //     p = html_quote(url_decode2(p, buf));
        //   else
        //     p = u;
        //   if (m->alt && *m->alt)
        //     t = html_quote(m->alt);
        //   else
        //     t = html_quote(url_decode2(m->url, buf));
        //   Strcat_m_charp(tmp, "<li><a href=\"", u, "\">", t, "</a><br>", p,
        //                  "\n", NULL);
        // }
        // Strcat_charp(tmp, "</ol>\n");
      }
    }
    Strcat_charp(tmp, "</ol>\n");
  }

  return loadHTMLString(tmp);
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
