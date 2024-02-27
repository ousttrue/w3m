#pragma once
#include "hash.h"
#include "textlist.h"
#include <string_view>
#include <gc_cpp.h>

typedef ListItem HistItem;
typedef GeneralList HistList;

struct Hist : public gc_cleanup {
  HistList *list = nullptr;
  HistItem *current = nullptr;
  Hash_sv *hash = nullptr;
  long long mtime = {};

private:
  Hist();

public:
  static Hist *newHist();
  bool loadHistory();
  void saveHistory(int size);
  Hist *copyHist() const;

  const char *nextHist();
  const char *prevHist();
  const char *lastHist();

  HistItem *pushHist(std::string_view ptr);

  HistItem *pushHashHist(const char *ptr);
  HistItem *getHashHist(const char *ptr);
  std::string toHtml() const;
};

extern Hist *LoadHist;
extern Hist *SaveHist;
extern Hist *URLHist;
extern Hist *ShellHist;
extern Hist *TextHist;
extern int UseHistory;
extern int URLHistSize;
extern int SaveURLHist;
