#pragma once
#include "Str.h"

#define SYMBOL_BASE 0x20
extern int symbol_width;
extern int symbol_width0;

struct Buffer;
void HTMLlineproc2body(struct Buffer *buf, Str (*feed)(), int llimit);
