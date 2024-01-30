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
  long long mtime;
};
extern Hist *LoadHist;
extern Hist *SaveHist;
extern Hist *URLHist;
extern Hist *ShellHist;
extern Hist *TextHist;
extern int UseHistory;
extern int URLHistSize;
extern int SaveURLHist;

Hist *newHist(void);
Hist *copyHist(Hist *hist);
HistItem *unshiftHist(Hist *hist, char *ptr);
HistItem *pushHist(Hist *hist, char *ptr);
HistItem *pushHashHist(Hist *hist, char *ptr);
HistItem *getHashHist(Hist *hist, char *ptr);
char *lastHist(Hist *hist);
char *nextHist(Hist *hist);
char *prevHist(Hist *hist);
int loadHistory(Hist *hist);
void saveHistory(Hist *hist, size_t size);
void ldHist(void);
