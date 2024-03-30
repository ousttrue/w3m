#pragma once
#include <ostream>

struct Str;
extern void push_symbol(Str *str, char symbol, int width, int n);

extern void push_symbol(std::ostream &os, char symbol, int width, int n);
