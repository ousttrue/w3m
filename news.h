#pragma once
#include <gcstr.h>
#include "Url.h"
#include <wc/wc.h>

Str loadNewsgroup(struct Url* pu, wc_ces* charset);
void closeNews();
void disconnectNews();
