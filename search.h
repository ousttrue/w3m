#pragma once
#include <wc.h>

struct Buffer;
char* conv_search_string(char* str, wc_ces f_ces);

enum SearchResult {
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
};

typedef enum SearchResult (*SearchFunc)(struct Buffer*, char*);

enum SearchResult forwardSearch(struct Buffer* buf, char* str);
enum SearchResult backwardSearch(struct Buffer* buf, char* str);
