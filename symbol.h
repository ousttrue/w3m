#pragma once
#include "Str.h"

#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)

extern char *graph_symbol[];
extern char *graph2_symbol[];

char **get_symbol();
void push_symbol(Str str, char symbol, int width, int n);
