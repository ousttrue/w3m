#pragma once
#include <wc.h>

struct _Buffer;
char* conv_search_string(char* str, wc_ces f_ces);
int forwardSearch(struct _Buffer* buf, char* str);
int backwardSearch(struct _Buffer* buf, char* str);
