#pragma once

#define SYMBOL_BASE 0x20

extern const char *graph_symbol[];
extern const char *graph2_symbol[];
extern const char **get_symbol(void);
struct Str;
extern void push_symbol(Str *str, char symbol, int width, int n);

struct Line;
Str *conv_symbol(Line *l);
