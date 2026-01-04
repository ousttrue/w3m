#pragma once
#include <libwc/ces.h>

const char* conv_search_string(const char* str, enum wc_ces f_ces, enum wc_ces doc_charset);

enum SearchResult {
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
};

struct Document;
typedef enum SearchResult (*SearchFunc)(struct Document*, const char*);

enum SearchResult forwardSearch(struct Document* buf, const char* str);
enum SearchResult backwardSearch(struct Document* buf, const char* str);

void srch(struct Document* doc, SearchFunc func, const char* prompt);
void isrch(struct Document* doc, SearchFunc func, const char* prompt);
void srch_nxtprv(struct Document *doc, int reverse);
