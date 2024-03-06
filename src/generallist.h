#pragma once
#include "alloc.h"
#define GENERAL_LIST_MAX (INT_MAX / 32)

struct ListItem {
  void *ptr;
  ListItem *next;
  ListItem *prev;
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
};

extern ListItem *newListItem(void *s, ListItem *n, ListItem *p);
extern void pushValue(GeneralList *tl, void *s);
extern void *popValue(GeneralList *tl);
extern void *rpopValue(GeneralList *tl);
extern void delValue(GeneralList *tl, ListItem *it);
extern GeneralList *appendGeneralList(GeneralList *, GeneralList *);
