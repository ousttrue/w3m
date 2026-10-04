#pragma once
#include "buffer.h"
#include "hash.h"

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127

typedef struct ListItem HistItem;

typedef struct GeneralList HistList;

typedef struct {
    HistList* list;
    HistItem* current;
    Hash_sv* hash;
    long long mtime;
} Hist;

extern Hist* newHist(void);
extern Hist* copyHist(Hist* hist);
extern HistItem* unshiftHist(Hist* hist, const char* ptr);
extern HistItem* pushHist(Hist* hist, const char* ptr);
extern HistItem* pushHashHist(Hist* hist, const char* ptr);
extern HistItem* getHashHist(Hist* hist, const char* ptr);
extern char* lastHist(Hist* hist);
extern char* nextHist(Hist* hist);
extern char* prevHist(Hist* hist);

extern Hist* LoadHist;
extern Hist* SaveHist;
extern Hist* URLHist;
extern Hist* ShellHist;
extern Hist* TextHist;

extern Buffer* historyBuffer(Hist* hist);
extern int loadUrlHistory(void);
extern void saveUrlHistory(void);
extern void ldHist(void);
extern void svHist(void);
