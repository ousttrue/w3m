#pragma once
#include <wc.h>

#define SYMBOL_BASE 0x20
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)

extern char* graph_symbol[];
extern char* graph2_symbol[];

char** get_symbol(wc_ces charset, int* width);
char** set_symbol(int width);
void push_symbol(Str str, char symbol, int width, int n);
void update_utf8_symbol(void);
