#pragma once
#include "textlist.h"

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127

typedef ListItem HistItem;
typedef GeneralList HistList;

struct Hist;
struct _Buffer;

struct Hist* newHist(void);
struct Hist* copyHist(struct Hist* hist);
HistItem* unshiftHist(struct Hist* hist, char* ptr);
HistItem* pushHist(struct Hist* hist, char* ptr);
HistItem* pushHashHist(struct Hist* hist, char* ptr);
HistItem* getHashHist(struct Hist* hist, char* ptr);
char* lastHist(struct Hist* hist);
char* nextHist(struct Hist* hist);
char* prevHist(struct Hist* hist);
int loadHistory(struct Hist* hist);
void saveHistory(struct Hist* hist, size_t size);
void ldHist(void);
struct _Buffer* historyBuffer(struct Hist* hist);
