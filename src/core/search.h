#pragma once
#include "buffer.h"

char* conv_search_string(char* str, wc_ces f_ces);
int forwardSearch(struct _Buffer* buf, char* str);
int backwardSearch(struct _Buffer* buf, char* str);
void isrch(int (*func)(Buffer*, char*), char* prompt);
void srch(int (*func)(Buffer*, char*), char* prompt);
void srch_nxtprv(int reverse);
