#pragma once
#include "Str.h"
#include <libwc/ces.h>
#include <stdbool.h>

struct Url;
union input_stream* openNewsStream(struct Url* pu);
Str loadNewsgroup(struct Url* pu, wc_ces* charset, bool do_download);
void closeNews(void);
void disconnectNews(void);
