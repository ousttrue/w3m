#include "anchor.h"
#include "readbuffer.h"
#include "rc.h"
#include "terms.h"
#include "linklist.h"
#include "httprequest.h"
#include "line.h"
#include "buffer.h"
#include "form.h"
#include "indep.h"
#include "myctype.h"
#include "regex.h"
#include "proto.h"
#include "alloc.h"

bool MarkAllPages = false;

#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */
int PagerMax = PAGER_MAX_LINE;

#define bpcmp(a, b)                                                            \
  (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

#define FIRST_ANCHOR_SIZE 30

AnchorList *putAnchor(AnchorList *al, const char *url, const char *target,
                      Anchor **anchor_return, const char *referer,
                      const char *title, unsigned char key, int line, int pos) {
  if (al == NULL) {
    al = new AnchorList;
  }

  BufferPoint bp = {0};
  bp.line = line;
  bp.pos = pos;

  size_t n = al->size();
  size_t i;
  if (!n || bpcmp(al->anchors[n - 1].start, bp) < 0) {
    i = n;
  } else {
    for (i = 0; i < n; i++) {
      if (bpcmp(al->anchors[i].start, bp) >= 0) {
        for (size_t j = n; j > i; j--)
          al->anchors[j] = al->anchors[j - 1];
        break;
      }
    }
  }

  while (i >= al->anchors.size()) {
    al->anchors.push_back({});
  }
  auto a = &al->anchors[i];
  a->url = url;
  a->target = target;
  a->referer = referer;
  a->title = title;
  a->accesskey = key;
  a->slave = FALSE;
  a->start = bp;
  a->end = bp;
  if (anchor_return)
    *anchor_return = a;
  return al;
}

Anchor *registerHref(Buffer *buf, const char *url, const char *target,
                     const char *referer, const char *title, unsigned char key,
                     int line, int pos) {
  Anchor *a;
  buf->href =
      putAnchor(buf->href, url, target, &a, referer, title, key, line, pos);
  return a;
}

Anchor *registerName(Buffer *buf, const char *url, int line, int pos) {
  Anchor *a;
  buf->name = putAnchor(buf->name, url, NULL, &a, NULL, NULL, '\0', line, pos);
  return a;
}

Anchor *registerImg(Buffer *buf, const char *url, const char *title, int line,
                    int pos) {
  Anchor *a;
  buf->img = putAnchor(buf->img, url, NULL, &a, NULL, title, '\0', line, pos);
  return a;
}

Anchor *registerForm(Buffer *buf, FormList *flist, HtmlTag *tag, int line,
                     int pos) {
  Anchor *a;
  FormItemList *fi;

  fi = formList_addInput(flist, tag);
  if (fi == NULL)
    return NULL;
  buf->formitem = putAnchor(buf->formitem, (const char *)fi, flist->target, &a,
                            NULL, NULL, '\0', line, pos);
  return a;
}

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
  if (!buf->currentLine || !buf->href)
    return NULL;
  return buf->href->retrieveAnchor(buf->currentLine->linenumber, buf->pos);
}

Anchor *retrieveCurrentImg(Buffer *buf) {
  if (!buf->currentLine || !buf->img)
    return NULL;
  return buf->img->retrieveAnchor(buf->currentLine->linenumber, buf->pos);
}

Anchor *retrieveCurrentForm(Buffer *buf) {
  if (!buf->currentLine || !buf->formitem)
    return NULL;
  return buf->formitem->retrieveAnchor(buf->currentLine->linenumber, buf->pos);
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

Anchor *searchURLLabel(Buffer *buf, const char *url) {
  return searchAnchor(buf->name, url);
}

static Anchor *_put_anchor_all(Buffer *buf, const char *p1, const char *p2,
                               int line, int pos) {
  Str *tmp;

  tmp = Strnew_charp_n(p1, p2 - p1);
  return registerHref(buf, url_quote(tmp->ptr), NULL, NO_REFERER, NULL, '\0',
                      line, pos);
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
  if (!buf->href)
    return;

  int nmark = (buf->hmarklist) ? buf->hmarklist->nmark : 0;
  int n = nmark;
  for (size_t i = 0; i < buf->href->size(); i++) {
    auto a = &buf->href->anchors[i];
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
  for (size_t i = 0; i < buf->href->size(); i++) {
    auto a = &buf->href->anchors[i];
    if (a->hseq == -2) {
      a->hseq = n;
      auto a1 =
          closest_next_anchor(buf->href, NULL, a->start.pos, a->start.line);
      a1 = closest_next_anchor(buf->formitem, a1, a->start.pos, a->start.line);
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
    ml = putHmarker(ml, buf->hmarklist->marks[i].line,
                    buf->hmarklist->marks[i].pos, seqmap[i]);
  }
  buf->hmarklist = ml;

  reseq_anchor0(buf->href, seqmap.data());
  reseq_anchor0(buf->formitem, seqmap.data());
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
  for (l = MarkAllPages ? buf->firstLine : buf->topLine;
       l != NULL &&
       (MarkAllPages || l->linenumber < buf->topLine->linenumber + LASTLINE);
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

void addMultirowsForm(Buffer *buf, AnchorList *al) {
  int j, k, col, ecol, pos;
  Anchor a_form, *a;
  Line *l, *ls;

  if (al == NULL || al->size() == 0)
    return;
  for (size_t i = 0; i < al->size(); i++) {
    a_form = al->anchors[i];
    al->anchors[i].rows = 1;
    if (a_form.hseq < 0 || a_form.rows <= 1)
      continue;
    for (l = buf->firstLine; l != NULL; l = l->next) {
      if (l->linenumber == a_form.y)
        break;
    }
    if (!l)
      continue;
    if (a_form.y == a_form.start.line)
      ls = l;
    else {
      for (ls = l; ls != NULL;
           ls = (a_form.y < a_form.start.line) ? ls->next : ls->prev) {
        if (ls->linenumber == a_form.start.line)
          break;
      }
      if (!ls)
        continue;
    }
    col = ls->bytePosToColumn(a_form.start.pos);
    ecol = ls->bytePosToColumn(a_form.end.pos);
    for (j = 0; l && j < a_form.rows; l = l->next, j++) {
      pos = l->columnPos(col);
      if (j == 0) {
        buf->hmarklist->marks[a_form.hseq].line = l->linenumber;
        buf->hmarklist->marks[a_form.hseq].pos = pos;
      }
      if (a_form.start.line == l->linenumber)
        continue;
      buf->formitem = putAnchor(buf->formitem, a_form.url, a_form.target, &a,
                                NULL, NULL, '\0', l->linenumber, pos);
      a->hseq = a_form.hseq;
      a->y = a_form.y;
      a->end.pos = pos + ecol - col;
      if (pos < 1 || a->end.pos >= l->size())
        continue;
      l->lineBuf[pos - 1] = '[';
      l->lineBuf[a->end.pos] = ']';
      for (k = pos; k < a->end.pos; k++)
        l->propBuf[k] |= PE_FORM;
    }
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
  l = buf->firstLine;
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

  if (buf->bufferprop & BP_INTERNAL ||
      (buf->linklist == NULL && buf->href == NULL && buf->img == NULL)) {
    return NULL;
  }

  if (buf->linklist) {
    Strcat_charp(tmp, "<hr><h2>Links</h2>\n<ol>\n");
    for (l = buf->linklist; l; l = l->next) {
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

  if (buf->href) {
    Strcat_charp(tmp, "<hr><h2>Anchors</h2>\n<ol>\n");
    al = buf->href;
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->hseq < 0 || a->slave)
        continue;
      pu = Url::parse2(a->url, baseURL(buf));
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

  if (buf->img) {
    Strcat_charp(tmp, "<hr><h2>Images</h2>\n<ol>\n");
    al = buf->img;
    for (size_t i = 0; i < al->size(); i++) {
      a = &al->anchors[i];
      if (a->slave)
        continue;
      pu = Url::parse2(a->url, baseURL(buf));
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
      if (!buf->formitem) {
        continue;
      }
      a = buf->formitem->retrieveAnchor(a->start.line, a->start.pos);
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
