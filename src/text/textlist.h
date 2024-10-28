#pragma once
#include "text/Str.h"
#include <limits.h>
#include <stdint.h>
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

struct ListItem *newListItem(void *s, struct ListItem *n, struct ListItem *p);
struct GeneralList *newGeneralList(void);
void pushValue(struct GeneralList *tl, void *s);
void *popValue(struct GeneralList *tl);
void *rpopValue(struct GeneralList *tl);
void delValue(struct GeneralList *tl, struct ListItem *it);
struct GeneralList *appendGeneralList(struct GeneralList *,
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

struct TextList *newTextList();

void pushText(struct TextList *tl, const char *s);
const char *popText(struct TextList *tl);
const char *rpopText(struct TextList *tl);
void delText(struct TextList *tl, void *i);
struct TextList *appendTextList(struct TextList *tl, struct TextList *tl2);
struct TextList *make_domain_list(char *domain_list);

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

struct TextLine *newTextLine(Str line, int pos);
void appendTextLine(struct TextLineList *tl, Str line, int pos);
struct TextLineList *newTextLineList();
void pushTextLine(struct TextLineList *tl, struct TextLine *lbuf);
struct TextLine *popTextLine(struct TextLineList *tl);
struct TextLine *rpopTextLine(struct TextLineList *tl);
struct TextLineList *appendTextLineList(struct TextLineList *tl,
                                        struct TextLineList *tl2);
