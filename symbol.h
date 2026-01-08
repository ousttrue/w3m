#pragma once
#include "Str.h"
#include <libwc/ces.h>

#define SYMBOL_BASE 0x20
#define N_GRAPH_SYMBOL 32
#define N_SYMBOL (N_GRAPH_SYMBOL + 14)

#define GRAPHIC_CHAR_ASCII 2
#define GRAPHIC_CHAR_DEC 1
#define GRAPHIC_CHAR_CHARSET 0

extern char* graph_symbol[];
extern char* graph2_symbol[];
extern int symbol_width;
extern int symbol_width0;

char** get_symbol(enum wc_ces charset, int* width);
char** set_symbol(int width);
void push_symbol(Str str, char symbol, int width, int n);
void update_utf8_symbol(void);
