#pragma once
#include "anchor.h"

struct HttpOption;

template <typename T> struct AnchorList {

private:
  int acache = -1;

public:
  std::vector<T> anchors;
  size_t size() const { return anchors.size(); }
  void clear() { anchors.clear(); }

  AnchorList() {}
  ~AnchorList() {}
  // AnchorList(const AnchorList &) = delete;
  // AnchorList &operator=(const AnchorList &) = delete;

  T *searchAnchor(std::string_view str) {
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

  T *retrieveAnchor(int line, int pos) {
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

  template <typename S> S *closest_next_anchor(S *an, int x, int y) {
    if (this->size() == 0)
      return an;
    for (size_t i = 0; i < this->size(); i++) {
      if (this->anchors[i].hseq < 0)
        continue;
      if (this->anchors[i].start.line > y ||
          (this->anchors[i].start.line == y &&
           this->anchors[i].start.pos > x)) {
        if (an == NULL || an->start.line > this->anchors[i].start.line ||
            (an->start.line == this->anchors[i].start.line &&
             an->start.pos > this->anchors[i].start.pos))
          an = &this->anchors[i];
      }
    }
    return an;
  }

  template <typename S> S *closest_prev_anchor(S *an, int x, int y) {
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

  void shiftAnchorPosition(HmarkerList *hl, int line, int pos, int shift) {
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
        if (auto po = hl->get(a->hseq)) {
          ;
          if (po->line == line) {
            po->pos = a->start.pos;
          }
        }
      }
      if (a->end.pos >= pos) {
        a->end.pos += shift;
      }
    }
  }

  void reseq_anchor0(short *seqmap) {
    for (size_t i = 0; i < this->size(); i++) {
      auto a = &this->anchors[i];
      if (a->hseq >= 0) {
        a->hseq = seqmap[a->hseq];
      }
    }
  }

  T *putAnchor(const T &a) {
    auto bp = a.start;
    size_t n = this->size();
    size_t i;
    if (!n || this->anchors[n - 1].start.cmp(bp) < 0) {
      i = n;
    } else {
      for (i = 0; i < n; i++) {
        if (this->anchors[i].start.cmp(bp) >= 0) {
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
    this->anchors[i] = a;
    return &this->anchors[i];
  }
};
