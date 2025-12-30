#pragma once
#include <libwc/ces.h>

const char* conv_search_string(const char* str, enum wc_ces f_ces);

enum SearchResult {
    SR_FOUND = 0x1,
    SR_NOTFOUND = 0x2,
    SR_WRAPPED = 0x4,
};

struct Buffer;
typedef enum SearchResult (*SearchFunc)(struct Buffer*, const char*);

enum SearchResult forwardSearch(struct Buffer* buf, const char* str);
enum SearchResult backwardSearch(struct Buffer* buf, const char* str);
