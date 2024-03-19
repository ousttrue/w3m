#pragma once
#include "url.h"
#include "line.h"
#include "html/anchorlist.h"
#include "html/form_item.h"
#include <memory>
#include <string>
#include <assert.h>

extern bool MarkAllPages;

struct HmarkerList;
struct HtmlTag;

using AnchorFunc = Anchor *(*)(struct LineData *, const char *, const char *,
                               int, int);

// Anchor *_put_anchor_all(LineData *layout, const char *p1, const char *p2,
//                         int line, int pos);

struct LineData {
  std::string title;

  Url baseURL;
  std::string baseTarget;

  // always reshape new buffers to mark URLs
  bool need_reshape = true;
  int refresh_interval = 0;
  std::vector<Line> lines;
  std::shared_ptr<AnchorList<Anchor>> _href;
  std::shared_ptr<AnchorList<Anchor>> _name;
  std::shared_ptr<AnchorList<Anchor>> _img;
  std::shared_ptr<AnchorList<FormAnchor>> _formitem;
  std::shared_ptr<HmarkerList> _hmarklist;
  struct LinkList *linklist = nullptr;
  std::vector<std::shared_ptr<Form>> formlist;
  std::shared_ptr<struct FormItem> form_submit;
  struct FormAnchor *submit = nullptr;

  LineData();
  Line *firstLine() { return lines.size() ? &lines.front() : nullptr; }
  Line *lastLine() { return lines.size() ? &lines.back() : nullptr; }
  const Line *lastLine() const {
    return lines.size() ? &lines.back() : nullptr;
  }
  int linenumber(const Line *t) const {
    for (int i = 0; i < (int)lines.size(); ++i) {
      if (&lines[i] == t) {
        return i;
      }
    }
    assert(false);
    return -1;
  }
  void clear();
  void addnewline(const char *line, Lineprop *prop, int byteLen);

  Anchor *registerName(const char *url, int line, int pos);
  Anchor *registerImg(const char *url, const char *title, int line, int pos);
  FormAnchor *registerForm(struct html_feed_environ *h_env,
                           const std::shared_ptr<Form> &flist, HtmlTag *tag,
                           const BufferPoint &bp);
  void addMultirowsForm();
  void reseq_anchor();
  // const char *reAnchorPos(Line *l, const char *p1, const char *p2,
  //                         AnchorFunc anchorproc);
  // const char *reAnchorAny(Line *topLine, const char *re, AnchorFunc
  // anchorproc); const char *reAnchor(Line *topLine, const char *re) {
  //   return this->reAnchorAny(topLine, re, _put_anchor_all);
  // }
  // const char *reAnchorWord(Line *l, int spos, int epos) {
  //   return this->reAnchorPos(l, &l->lineBuf()[spos], &l->lineBuf()[epos],
  //                            _put_anchor_all);
  // }

  const char *getAnchorText(Anchor *a);

  void addLink(struct HtmlTag *tag);
};
