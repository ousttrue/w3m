#pragma once
#include "Str.h"
#include <libwc/ces.h>
#include <stdbool.h>

struct Url;
struct URLFile;
union input_stream* openFTPStream(struct Url* pu, struct URLFile* uf);
Str loadFTPDir(struct Url* pu, wc_ces* charset, bool do_download);
void closeFTP(void);
void disconnectFTP(void);
