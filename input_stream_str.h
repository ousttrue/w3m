#pragma once
#include "growbuf.h"
#include "stream_encoding.h"

struct InputStream;

void ist_gets_to_growbuf(struct InputStream* stream, struct growbuf* gb, bool check_crnl);

struct InputStream* ist_decode(struct InputStream* is, enum StreamEncoding encoding);
