#pragma once
#include "libwc/ces.h"
#include "Str.h"

struct Url;
extern struct input_stream* openNewsStream(struct Url* pu);
extern pStr loadNewsgroup(struct Url* pu, wc_ces* charset);
extern void closeNews(void);
extern void disconnectNews(void);
