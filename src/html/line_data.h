#pragma once
#include "url.h"
#include "form.h"
#include "line.h"
#include "anchorlist.h"
#include <memory>
#include <string>
#include <assert.h>
#include <list>

extern bool MarkAllPages;

struct HmarkerList;
class HtmlTag;

using AnchorFunc = Anchor *(*)(struct LineData *, const char *, const char *,
                               int, int);

enum LinkType {
  LINK_TYPE_NONE = 0,
  LINK_TYPE_REL = 1,
  LINK_TYPE_REV = 2,
};

struct LinkList {
  std::string url;
  std::string title; /* Next, Contents, ... */
  std::string ctype; /* Content-Type */
  LinkType type;     /* Rel, Rev */
};

struct LineData {
  std::string title;

  Url baseURL;
  std::string baseTarget;

  int _reshapeCount = 0;
  int _cols = 0;

  // always reshape new buffers to mark URLs
  bool need_reshape = true;
  int refresh_interval = 0;
  std::vector<Line> lines;
  std::shared_ptr<AnchorList<Anchor>> _href;
  std::shared_ptr<AnchorList<Anchor>> _name;
  std::shared_ptr<AnchorList<Anchor>> _img;
  std::shared_ptr<AnchorList<FormAnchor>> _formitem;
  std::shared_ptr<HmarkerList> _hmarklist;
  std::list<LinkList> linklist;
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
  void clear(int cols);
  void addnewline(const char *line, const std::vector<Lineprop> &prop,
                  int byteLen);
  std::string status() const;

  Anchor *registerName(const char *url, int line, int pos);
  Anchor *registerImg(const std::string &url, const std::string &title, int line, int pos);
  FormAnchor *registerForm(const std::shared_ptr<FormItem> &fi,
                           const std::shared_ptr<Form> &flist,
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

  std::string getAnchorText(Anchor *a);

  void addLink(const std::shared_ptr<HtmlTag> &tag);
};
