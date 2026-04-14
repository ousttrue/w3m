#pragma once
#include "textlist.h"
#include "hash.h"

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

void initHist(void);
struct Hist* copyHist(struct Hist* hist);
HistItem* unshiftHist(struct Hist* hist, const char* ptr);
HistItem* pushHist(struct Hist* hist, const char* ptr);
HistItem* pushHashHist(struct Hist* hist, const char* ptr);
HistItem* getHashHist(struct Hist* hist, const char* ptr);
const char* lastHist(struct Hist* hist);
const char* nextHist(struct Hist* hist);
const char* prevHist(struct Hist* hist);
int loadHistory(struct Hist* hist);
struct CmdArgs;
void saveHistory(struct CmdArgs *args, struct Hist* hist, size_t size);
Str historyBuffer(struct Hist* hist);
