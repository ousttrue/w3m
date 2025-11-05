#pragma once
#include <gcstr/gcstr.h>
#include "Url.h"
#include <wc.h>

Str loadNewsgroup(struct Url* pu, wc_ces* charset);
void closeNews();
void disconnectNews();
