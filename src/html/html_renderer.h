#pragma once
#include "text/Str.h"

#define SYMBOL_BASE 0x20
extern int symbol_width;
extern int symbol_width0;
extern bool MetaRefresh;

typedef Str (*GetLineFunc)();
struct Url;
struct Document *HTMLlineproc2body(int cols, struct Url currentURL,
                                   struct Url *url, GetLineFunc feed);
