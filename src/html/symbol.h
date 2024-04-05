#pragma once
#include <string>

#define SYMBOL_BASE 0x20

extern const char *graph_symbol[];
extern const char *graph2_symbol[];
extern const char **get_symbol(void);

class Line;
std::string conv_symbol(Line *l);
