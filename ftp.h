#pragma once
#include "Str.h"
#include <libwc/wc_types.h>

typedef union input_stream* InputStream;
struct _ParsedURL;
struct URLFile;
InputStream openFTPStream(struct _ParsedURL* pu, struct URLFile* uf);
Str loadFTPDir(struct _ParsedURL* pu, wc_ces* charset);
void closeFTP(void);
void disconnectFTP(void);
