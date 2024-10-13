#pragma once
#include "hash.h"
#include "textlist.h"

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127

typedef struct ListItem HistItem;
typedef struct GeneralList HistList;
struct Hist {
  HistList *list;
  HistItem *current;
  Hash_sv *hash;
};
extern struct Hist *LoadHist;
extern struct Hist *SaveHist;
extern struct Hist *URLHist;
extern struct Hist *ShellHist;
extern struct Hist *TextHist;

extern struct Hist *newHist();
extern struct Hist *copyHist(struct Hist *hist);
extern HistItem *unshiftHist(struct Hist *hist, const char *ptr);
extern HistItem *pushHist(struct Hist *hist, const char *ptr);
extern HistItem *pushHashHist(struct Hist *hist, const char *ptr);
extern HistItem *getHashHist(struct Hist *hist, const char *ptr);
extern char *lastHist(struct Hist *hist);
extern char *nextHist(struct Hist *hist);
extern char *prevHist(struct Hist *hist);
extern struct Document *historyBuffer(struct Hist *hist);
extern void loadHistory(struct Hist *hist);
extern void saveHistory(struct Hist *hist, size_t size);
