#pragma once
#include "Str.h"
#include <libwc/ces.h>

Str decodeWord(char** ow, wc_ces* charset);
Str decodeMIME(Str orgstr, wc_ces* charset);
