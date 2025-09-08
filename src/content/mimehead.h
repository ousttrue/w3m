#pragma once

#include <Str.h>
#include <wc.h>

struct growbuf;

Str decodeB(char** ww);
void decodeB_to_growbuf(struct growbuf* gb, char** ww);
Str decodeQ(char** ww);
Str decodeQP(char** ww);
void decodeQP_to_growbuf(struct growbuf* gb, char** ww);
Str decodeU(char** ww);
void decodeU_to_growbuf(struct growbuf* gb, char** ww);
Str decodeWord(char** ow, wc_ces* charset);
Str decodeMIME(Str orgstr, wc_ces* charset);
