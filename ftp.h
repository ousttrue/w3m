#pragma once
#include <gcstr/gcstr.h>
#include "Url.h"
#include <wc.h>

Str loadFTPDir(struct Url* pu, wc_ces* charset);
void closeFTP();
void disconnectFTP();
