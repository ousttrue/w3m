#pragma once
#include "geometry.h"
#include <wc.h>
#include <stdbool.h>

extern char SearchConv;
extern int IgnoreCase;
extern int WrapSearch;
extern int show_srch_str;

enum SearchResultFlags {
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
};
typedef enum SearchResultFlags (*SearchFunc)(struct UI ui, const char*);

const char* conv_search_string(struct UI ui, const char* str, wc_ces f_ces);
enum SearchResultFlags forwardSearch(struct UI ui, const char* str);
enum SearchResultFlags backwardSearch(struct UI ui, const char* str);
void isrch(struct UI ui, SearchFunc func, char* prompt);
void srch(struct UI ui, SearchFunc func, char* prompt);
void srch_nxtprv(struct UI ui, bool reverse);
