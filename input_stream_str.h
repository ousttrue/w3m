#pragma once
#include "Str.h"
#include "growbuf.h"
#include "stream_encoding.h"

struct InputStream;
Str StrISgets2(struct InputStream* stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)

void ist_gets_to_growbuf(struct InputStream* stream, struct growbuf* gb, bool check_crnl);

struct InputStream* ist_decode(struct InputStream* is, enum StreamEncoding encoding);
