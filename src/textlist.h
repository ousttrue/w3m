#pragma once
#include "Str.h"
#include <limits.h>
#include <string>
#include <list>
#define GENERAL_LIST_MAX (INT_MAX / 32)

/* General doubly linked list */

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

/* Text list */

struct TextListItem {
  char *ptr;
  TextListItem *next;
  TextListItem *prev;
};

struct TextList {
  TextListItem *first;
  TextListItem *last;
  int nitem;
};

#define newTextList() ((TextList *)newGeneralList())
#define pushText(tl, s)                                                        \
  pushValue((GeneralList *)(tl), (void *)allocStr((s), -1))
#define popText(tl) ((char *)popValue((GeneralList *)(tl)))
#define rpopText(tl) ((char *)rpopValue((GeneralList *)(tl)))
#define delText(tl, i) delValue((GeneralList *)(tl), (ListItem *)(i))
#define appendTextList(tl, tl2)                                                \
  ((TextList *)appendGeneralList((GeneralList *)(tl), (GeneralList *)(tl2)))
