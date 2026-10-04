#pragma once
#include "Str.h"
#include "libwc/ces.h"

struct Url;
struct UrlStream;
struct input_stream* openFTPStream(struct Url* pu, struct UrlStream* uf);
pStr loadFTPDir(struct Url* pu, wc_ces* charset);
void closeFTP(void);
void disconnectFTP(void);
