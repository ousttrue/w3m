#pragma once
#include "textlist.h"
#include "hash.h"
#include <stddef.h>

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

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127

struct Buffer;

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
struct Buffer* historyBuffer(struct Hist* hist);
