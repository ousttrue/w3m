#pragma once
#include "buffer.h"
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
typedef enum SearchResultFlags (*SearchFunc)(Buffer*, char*);

char* conv_search_string(char* str, wc_ces f_ces);
enum SearchResultFlags forwardSearch(struct _Buffer* buf, char* str);
enum SearchResultFlags backwardSearch(struct _Buffer* buf, char* str);
void isrch(SearchFunc func, char* prompt);
void srch(SearchFunc func, char* prompt);
void srch_nxtprv(bool reverse);
