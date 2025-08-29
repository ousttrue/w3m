#pragma once
#include <wc.h>

char** get_symbol(wc_ces charset, int* width);
char** set_symbol(int width);
void push_symbol(Str str, char symbol, int width, int n);
void update_utf8_symbol(void);
