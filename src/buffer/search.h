#pragma once

extern bool IgnoreCase;
extern bool WrapSearch;
extern bool show_srch_str;

enum SearchResult {
  SR_FOUND = 0x1,
  SR_NOTFOUND = 0x2,
  SR_WRAPPED = 0x4,
};

struct Document;

typedef enum SearchResult (*SearchRoutine)(struct Document *, const char *);
extern enum SearchResult forwardSearch(struct Document *doc, const char *str);
extern enum SearchResult backwardSearch(struct Document *doc, const char *str);

void srch(struct Document *doc, SearchRoutine func, const char *prompt);
void isrch(struct Document *doc, SearchRoutine func, const char *prompt);
void srch_nxtprv(struct Document *doc, bool reverse);
