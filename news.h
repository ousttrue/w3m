#pragma once
#include <w3m.h>
#include "Str.h"
#include <libwc/wc_types.h>

typedef union input_stream* InputStream;
struct Url;
InputStream openNewsStream(struct Url* pu);
Str loadNewsgroup(struct CmdArgs *args, struct Url* pu, wc_ces* charset);
void closeNews(void);
void disconnectNews(void);
