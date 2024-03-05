#pragma once

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
