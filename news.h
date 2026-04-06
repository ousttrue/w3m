#pragma once
#include "Str.h"
#include <libwc/wc_types.h>

typedef union input_stream* InputStream;
struct Url;
InputStream openNewsStream(struct Url* pu);
Str loadNewsgroup(struct Url* pu, wc_ces* charset);
void closeNews(void);
void disconnectNews(void);
