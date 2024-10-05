#pragma once
#include "Str.h"

Str decodeB(char **ww);
Str decodeQ(char **ww);
Str decodeQP(char **ww);
Str decodeU(char **ww);
Str decodeWord0(char **ow);
Str decodeMIME0(Str orgstr);
#define decodeWord(ow, charset) decodeWord0(ow)
#define decodeMIME(orgstr, charset) decodeMIME0(orgstr)
Str encodeB(char *a);
