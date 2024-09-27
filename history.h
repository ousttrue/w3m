#pragma once
#include "textlist.h"
#include "hash.h"

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127

typedef ListItem HistItem;
typedef GeneralList HistList;
struct Hist {
  HistList *list;
  HistItem *current;
  Hash_sv *hash;
};

extern struct Hist *newHist();
extern struct Hist *copyHist(struct Hist *hist);
extern HistItem *unshiftHist(struct Hist *hist, char *ptr);
extern HistItem *pushHist(struct Hist *hist, char *ptr);
extern HistItem *pushHashHist(struct Hist *hist, char *ptr);
extern HistItem *getHashHist(struct Hist *hist, char *ptr);
extern char *lastHist(struct Hist *hist);
extern char *nextHist(struct Hist *hist);
extern char *prevHist(struct Hist *hist);
