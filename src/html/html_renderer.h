#pragma once
#include "Str.h"

#define SYMBOL_BASE 0x20
extern int symbol_width;
extern int symbol_width0;

typedef Str (*GetLineFunc)();
struct Url;
struct Document *HTMLlineproc2body(struct Url currentURL, struct Url *url,
                                   GetLineFunc feed);
