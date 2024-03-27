#pragma once

#define N_GRAPH_SYMBOL 32
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)

#define SYMBOL_BASE 0x20

extern const char *graph_symbol[];
extern const char *graph2_symbol[];
extern const char **get_symbol(void);
struct Str;
extern void push_symbol(Str *str, char symbol, int width, int n);

class Line;
Str *conv_symbol(Line *l);
