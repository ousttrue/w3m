#pragma once
#include "textlist.h"
#include "hash.h"
#include <string_view>

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
HistItem *unshiftHist(Hist *hist, const char *ptr);
HistItem *pushHist(Hist *hist, std::string_view ptr);
HistItem *pushHashHist(Hist *hist, const char *ptr);
HistItem *getHashHist(Hist *hist, const char *ptr);
const char *lastHist(Hist *hist);
const char *nextHist(Hist *hist);
const char *prevHist(Hist *hist);
int loadHistory(Hist *hist);
void saveHistory(Hist *hist, int size);
void ldHist(void);
struct Buffer;
Buffer *historyBuffer(Hist *hist);
