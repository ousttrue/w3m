#pragma once
#include "textlist.h"
#include <gcstr/hash.h>

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127

typedef ListItem HistItem;

typedef GeneralList HistList;

struct Hist {
    HistList* list;
    HistItem* current;
    Hash_sv* hash;
    long long mtime;
};

extern struct Hist* LoadHist;
extern struct Hist* SaveHist;
extern struct Hist* URLHist;
extern struct Hist* ShellHist;
extern struct Hist* TextHist;
extern int UseHistory;
extern int URLHistSize;
extern int SaveURLHist;
extern int multicolList;

extern struct Hist* newHist(void);
extern struct Hist* copyHist(struct Hist* hist);
extern HistItem* unshiftHist(struct Hist* hist, char* ptr);
extern HistItem* pushHist(struct Hist* hist, char* ptr);
extern HistItem* pushHashHist(struct Hist* hist, char* ptr);
extern HistItem* getHashHist(struct Hist* hist, char* ptr);
extern char* lastHist(struct Hist* hist);
extern char* nextHist(struct Hist* hist);
extern char* prevHist(struct Hist* hist);

extern int loadHistory(struct Hist* hist);
extern void saveHistory(struct Hist* hist, size_t size);
extern void ldHist(void);
