#include "line_data.h"
#include "Str.h"
#include "myctype.h"
#include "quote.h"
#include "form.h"
#include "etc.h"
#include "html/html_tag.h"
#include "url_quote.h"
#include "alloc.h"
#include "html/html_feed_env.h"
#include <sstream>

bool MarkAllPages = false;

LineData::LineData() {
  this->_href = std::make_shared<AnchorList<Anchor>>();
  this->_name = std::make_shared<AnchorList<Anchor>>();
  this->_img = std::make_shared<AnchorList<Anchor>>();
  this->_formitem = std::make_shared<AnchorList<FormAnchor>>();
  this->_hmarklist = std::make_shared<HmarkerList>();
}

std::string LineData::status() const {
  std::stringstream ss;
  ss << "#"
     << _reshapeCount
     //
     << ", width: "
     << _cols
     //
     << ", lines: " << lines.size()
      //
      ;
  return ss.str();
}

void LineData::addnewline(const char *line, Lineprop *prop, int byteLen) {
  lines.push_back({line, prop, byteLen});
}

FormAnchor *LineData::registerForm(html_feed_environ *h_env,
                                   const std::shared_ptr<Form> &flist,
                                   const std::shared_ptr<HtmlTag> &tag,
                                   const BufferPoint &bp) {
  auto fi = h_env->createFormItem(tag);
  if (!fi) {
    return NULL;
  }

  fi->parent = flist;
  if (fi->type == FORM_INPUT_HIDDEN) {
    return NULL;
  }
  flist->items.push_back(fi);
  return this->_formitem->putAnchor(FormAnchor(bp, fi));
}

void LineData::addMultirowsForm() {

  if (_formitem->size() == 0)
    return;

  for (size_t i = 0; i < _formitem->size(); i++) {
    auto a_form = _formitem->anchors[i];
    _formitem->anchors[i].rows = 1;
    if (a_form.hseq < 0 || a_form.rows <= 1)
      continue;

    Line *l;
    for (l = firstLine(); l != NULL; ++l) {
      if (linenumber(l) == a_form.y)
        break;
    }
    if (!l)
      continue;

    Line *ls;
    if (a_form.y == a_form.start.line)
      ls = l;
    else {
      for (ls = l; ls != NULL; (a_form.y < a_form.start.line) ? ++ls : --ls) {
        if (linenumber(ls) == a_form.start.line)
          break;
      }
      if (!ls)
        continue;
    }

    auto col = ls->bytePosToColumn(a_form.start.pos);
    auto ecol = ls->bytePosToColumn(a_form.end.pos);
    for (int j = 0; l && j < a_form.rows; ++l, j++) {
      auto pos = l->columnPos(col);
      if (j == 0) {
        this->_hmarklist->set(a_form.hseq, linenumber(l), pos);
      }
      if (a_form.start.line == linenumber(l))
        continue;

      BufferPoint bp{
          .line = linenumber(l),
          .pos = pos,
      };
      auto a = this->_formitem->putAnchor(FormAnchor(bp, a_form.formItem));
      a->end.pos = pos + ecol - col;
      if (pos < 1 || a->end.pos >= l->len())
        continue;
      l->lineBuf(pos - 1, '[');
      l->lineBuf(a->end.pos, ']');
      for (int k = pos; k < a->end.pos; k++) {
        l->propBufAdd(k, PE_FORM);
      }
    }
  }
}

/* renumber anchor */
void LineData::reseq_anchor() {
  int nmark = this->_hmarklist->size();
  int n = nmark;
  for (size_t i = 0; i < this->_href->size(); i++) {
    auto a = &this->_href->anchors[i];
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
  for (size_t i = 0; i < this->_href->size(); i++) {
    auto a = &this->_href->anchors[i];
    if (a->hseq == -2) {
      a->hseq = n;
      auto a1 = this->_href->closest_next_anchor((Anchor *)NULL, a->start.pos,
                                                 a->start.line);
      a1 =
          this->_formitem->closest_next_anchor(a1, a->start.pos, a->start.line);
      if (a1 && a1->hseq >= 0) {
        seqmap[n] = seqmap[a1->hseq];
        for (int j = a1->hseq; j < nmark; j++) {
          seqmap[j]++;
        }
      }
      this->_hmarklist->putHmarker(a->start.line, a->start.pos, seqmap[n]);
      n++;
    }
  }

  for (int i = 0; i < nmark; i++) {
    auto po = this->_hmarklist->get(i);
    this->_hmarklist->putHmarker(po->line, po->pos, seqmap[i]);
  }

  this->_href->reseq_anchor0(seqmap.data());
  this->_formitem->reseq_anchor0(seqmap.data());
}

// const char *LineData ::reAnchorPos(
//     Line *l, const char *p1, const char *p2,
//     Anchor *(*anchorproc)(LineData *, const char *, const char *, int, int))
//     {
//   Anchor *a;
//   int i;
//   int hseq = -2;
//
//   auto spos = p1 - l->lineBuf();
//   auto epos = p2 - l->lineBuf();
//   for (i = spos; i < epos; i++) {
//     if (l->propBuf()[i] & (PE_ANCHOR | PE_FORM))
//       return p2;
//   }
//   for (i = spos; i < epos; i++) {
//     l->propBufAdd(i, PE_ANCHOR);
//   }
//
//   while (1) {
//     a = anchorproc(this, p1, p2, linenumber(l), spos);
//     a->hseq = hseq;
//     if (hseq == -2) {
//       this->reseq_anchor();
//       hseq = a->hseq;
//     }
//     a->end.line = linenumber(l);
//     a->end.pos = epos;
//     break;
//   }
//   return p2;
// }

/* search regexp and register them as anchors */
/* returns error message if any               */
// const char *LineData::reAnchorAny(
//     Line *topLine, const char *re,
//     Anchor *(*anchorproc)(LineData *, const char *, const char *, int, int))
//     {
//
//   if (re == NULL || *re == '\0') {
//     return NULL;
//   }
//   if ((re = regexCompile(re, 1)) != NULL) {
//     return re;
//   }
//
//   Line *l;
//   for (l = MarkAllPages ? this->firstLine() : topLine;
//        l != NULL &&
//        (MarkAllPages ||
//         linenumber(l) < linenumber(topLine) + App::instance().LASTLINE());
//        ++l) {
//     auto p = l->lineBuf();
//     for (;;) {
//       if (regexMatch(p, &l->lineBuf()[l->len()] - p, p == l->lineBuf()) == 1)
//       {
//         const char *p1, *p2;
//         matchedPosition(&p1, &p2);
//         p = this->reAnchorPos(l, p1, p2, anchorproc);
//       } else
//         break;
//     }
//   }
//   return NULL;
// }

// Anchor *_put_anchor_all(LineData *layout, const char *p1, const char *p2,
//                         int line, int pos) {
//   auto tmp = Strnew_charp_n(p1, p2 - p1);
//   return layout->_href->putAnchor(
//       {
//           .line = line,
//           .pos = pos,
//       },
//       Anchor{
//           .url = url_quote(tmp->ptr).c_str(),
//           .target = "",
//           .option = {.no_referer = true},
//           .title = "",
//           .accesskey = '\0',
//       });
// }

const char *LineData::getAnchorText(Anchor *a) {
  if (!a || a->hseq < 0)
    return NULL;

  Str *tmp = NULL;
  auto hseq = a->hseq;
  auto l = this->lines.begin();
  for (size_t i = 0; i < _href->size(); i++) {
    a = &_href->anchors[i];
    if (a->hseq != hseq)
      continue;
    for (; l != lines.end(); ++l) {
      if (linenumber(&*l) == a->start.line)
        break;
    }
    if (l == lines.end())
      break;
    auto p = l->lineBuf() + a->start.pos;
    auto ep = l->lineBuf() + a->end.pos;
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

void LineData::addLink(const std::shared_ptr<HtmlTag> &tag) {
  const char *href = NULL;
  const char *title = NULL;
  const char *ctype = NULL;
  const char *rel = NULL;
  const char *rev = NULL;
  LinkType type = LINK_TYPE_NONE;
  // LinkList *l;

  tag->parsedtag_get_value(ATTR_HREF, &href);
  if (href)
    href = Strnew(url_quote(remove_space(href)))->ptr;
  tag->parsedtag_get_value(ATTR_TITLE, &title);
  tag->parsedtag_get_value(ATTR_TYPE, &ctype);
  tag->parsedtag_get_value(ATTR_REL, &rel);
  if (rel != NULL) {
    /* forward link type */
    type = LINK_TYPE_REL;
    if (title == NULL)
      title = rel;
  }
  tag->parsedtag_get_value(ATTR_REV, &rev);
  if (rev != NULL) {
    /* reverse link type */
    type = LINK_TYPE_REV;
    if (title == NULL)
      title = rev;
  }

  linklist.push_back({
      .url = href,
      .title = title,
      .ctype = ctype ? ctype : "",
      .type = type,
  });
}

void LineData::clear(int cols) {
  need_reshape = true;
  lines.clear();
  this->_href->clear();
  this->_name->clear();
  this->_img->clear();
  this->_formitem->clear();
  this->formlist.clear();
  this->linklist.clear();
  this->_hmarklist->clear();
  ++_reshapeCount;
  _cols = cols;
}

Anchor *LineData::registerName(const char *url, int line, int pos) {
  BufferPoint bp{
      .line = line,
      .pos = pos,
  };
  return this->_name->putAnchor(Anchor{
      .url = url,
      .target = "",
      .option = {},
      .title = "",
      .accesskey = '\0',
      .start = bp,
      .end = bp,
  });
}

Anchor *LineData::registerImg(const char *url, const char *title, int line,
                              int pos) {
  BufferPoint bp{
      .line = line,
      .pos = pos,
  };
  return this->_img->putAnchor(Anchor{
      .url = url,
      .target = "",
      .option = {},
      .title = title ? title : "",
      .accesskey = '\0',
      .start = bp,
      .end = bp,
  });
}
