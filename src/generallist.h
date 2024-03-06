#pragma once
#include "alloc.h"
#define GENERAL_LIST_MAX (INT_MAX / 32)

struct ListItem {
  void *ptr;
  ListItem *next;
  ListItem *prev;
  static ListItem *newListItem(void *s, ListItem *n, ListItem *p) {
    auto it = (ListItem *)New(ListItem);
    it->ptr = s;
    it->next = n;
    it->prev = p;
    return it;
  }
};

struct GeneralList {
  ListItem *first;
  ListItem *last;
  int nitem;

  static GeneralList *newGeneralList() {
    auto tl = (GeneralList *)New(GeneralList);
    tl->first = tl->last = NULL;
    tl->nitem = 0;
    return tl;
  }

  void pushValue(void *s) {
    ListItem *it;
    if (s == NULL /*|| tl == NULL*/ || this->nitem >= GENERAL_LIST_MAX)
      return;
    it = ListItem::newListItem(s, NULL, this->last);
    if (this->first == NULL) {
      this->first = it;
      this->last = it;
      this->nitem = 1;
    } else {
      this->last->next = it;
      this->last = it;
      this->nitem++;
    }
  }

  void *popValue() {
    if (!this->first) {
      return nullptr;
    }
    auto f = this->first;
    this->first = f->next;
    if (this->first)
      this->first->prev = NULL;
    else
      this->last = NULL;
    this->nitem--;
    return f->ptr;
  }

  void *rpopValue() {
    if (this->last == NULL)
      return NULL;
    auto f = this->last;
    this->last = f->prev;
    if (this->last)
      this->last->next = NULL;
    else
      this->first = NULL;
    this->nitem--;
    return f->ptr;
  }

  void delValue(ListItem *it) {
    if (it->prev)
      it->prev->next = it->next;
    else
      this->first = it->next;
    if (it->next)
      it->next->prev = it->prev;
    else
      this->last = it->prev;
    this->nitem--;
  }

  void appendGeneralList(GeneralList *tl2) {
    if (/*tl &&*/ tl2) {
      if (tl2->first) {
        if (this->last) {
          if (this->nitem + tl2->nitem > GENERAL_LIST_MAX) {
            return;
          }
          this->last->next = tl2->first;
          tl2->first->prev = this->last;
          this->last = tl2->last;
          this->nitem += tl2->nitem;
        } else {
          this->first = tl2->first;
          this->last = tl2->last;
          this->nitem = tl2->nitem;
        }
      }
      tl2->first = tl2->last = NULL;
      tl2->nitem = 0;
    }
  }
};
