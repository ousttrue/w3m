#pragma once

enum SearchResult {
  SR_FOUND = 0x1,
  SR_NOTFOUND = 0x2,
  SR_WRAPPED = 0x4,
};

struct Buffer;

typedef enum SearchResult (*SearchRoutine)(struct Buffer *, const char *);
extern enum SearchResult forwardSearch(struct Buffer *buf, const char *str);
extern enum SearchResult backwardSearch(struct Buffer *buf, const char *str);

void srch(SearchRoutine func, const char *prompt);
void isrch(SearchRoutine func, const char *prompt);
void srch_nxtprv(bool reverse);
