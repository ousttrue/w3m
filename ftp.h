#pragma once
#include <w3m.h>
#include "Str.h"
#include <libwc/wc_types.h>

struct InputStream;
struct Url;
struct URLFile;
struct InputStream *openFTPStream(struct CmdArgs *args, struct Url* pu, struct URLFile* uf);
Str loadFTPDir(struct Url* pu, wc_ces* charset);
void closeFTP(void);
void disconnectFTP(void);
