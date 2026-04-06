#pragma once
#include "line.h"
#include "Str.h"
#include <libwc/wc_types.h>

// Search Result
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

const char* conv_search_string(const char* str, wc_ces f_ces);
struct Buffer;
int forwardSearch(struct Buffer* buf, const char* str);
int backwardSearch(struct Buffer* buf, const char* str);

typedef int (*SrchFunc)(struct Buffer*, const char*);
void srch(SrchFunc func, const char* prompt);
void isrch(SrchFunc func, const char* prompt);
void srch_nxtprv(int reverse);
int srchcore(const char* str, SrchFunc func);
int dispincsrch(int ch, Str buf, Lineprop* prop);
