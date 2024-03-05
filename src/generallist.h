#pragma once
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
};

extern ListItem *newListItem(void *s, ListItem *n, ListItem *p);
extern GeneralList *newGeneralList(void);
extern void pushValue(GeneralList *tl, void *s);
extern void *popValue(GeneralList *tl);
extern void *rpopValue(GeneralList *tl);
extern void delValue(GeneralList *tl, ListItem *it);
extern GeneralList *appendGeneralList(GeneralList *, GeneralList *);
