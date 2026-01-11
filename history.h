#pragma once
#include "textlist.h"

typedef ListItem HistItem;
typedef GeneralList HistList;

struct Hist* newHist(void);
struct Hist* copyHist(struct Hist* hist);
HistItem* unshiftHist(struct Hist* hist, const char* ptr);
HistItem* pushHist(struct Hist* hist, const char* ptr);
HistItem* pushHashHist(struct Hist* hist, const char* ptr);
HistItem* getHashHist(struct Hist* hist, char* ptr);
char* lastHist(struct Hist* hist);
char* nextHist(struct Hist* hist);
char* prevHist(struct Hist* hist);
int loadHistory(struct Hist* hist);
void saveHistory(struct Hist* hist, size_t size);
struct Buffer* historyBuffer(struct Hist* hist);
