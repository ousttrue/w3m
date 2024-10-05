#pragma once
#include "text/Str.h"
#include <limits.h>
#define GENERAL_LIST_MAX (INT_MAX / 32)

/* General doubly linked list */

struct ListItem {
  void *ptr;
  struct ListItem *next;
  struct ListItem *prev;
};

struct GeneralList {
  struct ListItem *first;
  struct ListItem *last;
  int nitem;
};

extern struct ListItem *newListItem(void *s, struct ListItem *n,
                                    struct ListItem *p);
extern struct GeneralList *newGeneralList(void);
extern void pushValue(struct GeneralList *tl, void *s);
extern void *popValue(struct GeneralList *tl);
extern void *rpopValue(struct GeneralList *tl);
extern void delValue(struct GeneralList *tl, struct ListItem *it);
extern struct GeneralList *appendGeneralList(struct GeneralList *,
                                             struct GeneralList *);

/* Text list */

struct TextListItem {
  char *ptr;
  struct TextListItem *next;
  struct TextListItem *prev;
};

struct TextList {
  struct TextListItem *first;
  struct TextListItem *last;
  int nitem;
};

#define newTextList() ((struct TextList *)newGeneralList())
#define pushText(tl, s)                                                        \
  pushValue((struct GeneralList *)(tl), (void *)allocStr((s), -1))
#define popText(tl) ((char *)popValue((struct GeneralList *)(tl)))
#define rpopText(tl) ((char *)rpopValue((struct GeneralList *)(tl)))
#define delText(tl, i) delValue((struct GeneralList *)(tl), (void *)(i))
#define appendTextList(tl, tl2)                                                \
  ((struct TextList *)appendGeneralList((struct GeneralList *)(tl),            \
                                        (struct GeneralList *)(tl2)))

/* Line text list */

struct TextLine {
  Str line;
  int pos;
};

struct TextLineListItem {
  struct TextLine *ptr;
  struct TextLineListItem *next;
  struct TextLineListItem *prev;
};

struct TextLineList {
  struct TextLineListItem *first;
  struct TextLineListItem *last;
  int nitem;
};

extern struct TextLine *newTextLine(Str line, int pos);
extern void appendTextLine(struct TextLineList *tl, Str line, int pos);
#define newTextLineList() ((struct TextLineList *)newGeneralList())
#define pushTextLine(tl, lbuf)                                                 \
  pushValue((struct GeneralList *)(tl), (void *)(lbuf))
#define popTextLine(tl)                                                        \
  ((struct TextLine *)popValue((struct GeneralList *)(tl)))
#define rpopTextLine(tl)                                                       \
  ((struct TextLine *)rpopValue((struct GeneralList *)(tl)))
#define appendTextLineList(tl, tl2)                                            \
  ((struct TextLineList *)appendGeneralList((struct GeneralList *)(tl),        \
                                            (struct GeneralList *)(tl2)))
