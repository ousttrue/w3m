#pragma once
#include <gcstr.h>
#include "Url.h"
#include <wc/wc.h>

Str loadFTPDir(struct Url* pu, wc_ces* charset);
void closeFTP();
void disconnectFTP();
