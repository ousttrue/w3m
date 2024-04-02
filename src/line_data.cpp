#include "line_data.h"
#include "myctype.h"
#include "quote.h"
#include "form.h"
#include "html_tag.h"
#include "url_quote.h"
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

void LineData::addnewline(const char *line, const std::vector<Lineprop> &prop,
                          int byteLen) {
  lines.push_back({line, prop.data(), byteLen});
}

FormAnchor *LineData::registerForm(const std::shared_ptr<FormItem> &fi,
                                   const std::shared_ptr<Form> &flist,
                                   const BufferPoint &bp) {
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

std::string LineData::getAnchorText(Anchor *a) {
  if (!a || a->hseq < 0) {
    return {};
  }

  std::stringstream tmp;
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
    tmp << ' ';
    tmp << std::string_view(p, ep - p);
  }
  return tmp.str();
}

void LineData::addLink(const std::shared_ptr<HtmlTag> &tag) {
  std::string href;
  if (auto value = tag->getAttr(ATTR_HREF)) {
    href = *value;
  }
  if (href.size()) {
    href = url_quote(remove_space(href));
  }

  std::string title;
  if (auto value = tag->getAttr(ATTR_TITLE)) {
    title = *value;
  }

  std::string ctype;
  if (auto value = tag->getAttr(ATTR_TYPE)) {
    ctype = *value;
  }

  std::string rel;
  if (auto value = tag->getAttr(ATTR_REL)) {
    rel = *value;
  }

  LinkType type = LINK_TYPE_NONE;
  if (rel.size()) {
    /* forward link type */
    type = LINK_TYPE_REL;
    if (title.empty()) {
      title = rel;
    }
  }

  std::string rev;
  if (auto value = tag->getAttr(ATTR_REV)) {
    rev = *value;
  }
  if (rev.size()) {
    /* reverse link type */
    type = LINK_TYPE_REV;
    if (title.empty()) {
      title = rev;
    }
  }

  linklist.push_back({
      .url = href,
      .title = title,
      .ctype = ctype,
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

Anchor *LineData::registerImg(const std::string &url, const std::string &title,
                              int line, int pos) {
  BufferPoint bp{
      .line = line,
      .pos = pos,
  };
  return this->_img->putAnchor(Anchor{
      .url = url,
      .target = "",
      .option = {},
      .title = title,
      .accesskey = '\0',
      .start = bp,
      .end = bp,
  });
}
