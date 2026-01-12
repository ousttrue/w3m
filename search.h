#pragma once
#include "defun.h"
#include <libwc/ces.h>

const char* conv_search_string(const char* str, enum wc_ces f_ces, enum wc_ces doc_charset);

enum SearchResult {
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
};

typedef enum SearchResult (*SearchFunc)(struct DefunContext, const char*);

enum SearchResult forwardSearch(struct DefunContext ctx, const char* str);
enum SearchResult backwardSearch(struct DefunContext ctx, const char* str);

void srch(struct DefunContext ctx, SearchFunc func, const char* prompt);
void isrch(struct DefunContext ctx, SearchFunc func, const char* prompt);
void srch_nxtprv(struct DefunContext ctx, int reverse);
