#pragma once

#include "line.h"
#include <memory>
#include <string>
#include <assert.h>

struct AnchorList;
struct Anchor;
struct HmarkerList;
struct HtmlTag;

using AnchorFunc = Anchor *(*)(struct LineData *, const char *, const char *,
                               int, int);

struct LineData {
  std::string title;
  // always reshape new buffers to mark URLs
  bool need_reshape = true;
  int refresh_interval = 0;
  std::vector<Line> lines;
  std::shared_ptr<AnchorList> _href;
  std::shared_ptr<AnchorList> _name;
  std::shared_ptr<AnchorList> _img;
  std::shared_ptr<AnchorList> _formitem;
  std::shared_ptr<HmarkerList> _hmarklist;
  std::shared_ptr<HmarkerList> _imarklist;
  struct LinkList *linklist = nullptr;
  struct FormList *formlist = nullptr;
  struct FormItemList *form_submit = nullptr;
  struct Anchor *submit = nullptr;

  LineData();
  Line *firstLine() { return lines.size() ? &lines.front() : nullptr; }
  Line *lastLine() { return lines.size() ? &lines.back() : nullptr; }
  int linenumber(Line *t) const {
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
  Anchor *registerForm(class HtmlParser *parser, FormList *flist, HtmlTag *tag,
                       int line, int pos);
  void addMultirowsForm();
  void reseq_anchor();
  const char *reAnchorPos(Line *l, const char *p1, const char *p2,
                          AnchorFunc anchorproc);
  const char *reAnchorAny(Line *topLine, const char *re, AnchorFunc anchorproc);
  const char *reAnchor(Line *topLine, const char *re);
  const char *reAnchorWord(Line *l, int spos, int epos);
  const char *getAnchorText(Anchor *a);

  void addLink(struct HtmlTag *tag);
};
