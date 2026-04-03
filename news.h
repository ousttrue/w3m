#pragma once
#include "Str.h"
#include <wc.h>

typedef union input_stream* InputStream;
struct _ParsedURL;
InputStream openNewsStream(struct _ParsedURL* pu);
Str loadNewsgroup(struct _ParsedURL* pu, wc_ces* charset);
void closeNews(void);
void disconnectNews(void);
