#pragma once
#include "Str.h"
#include <libwc/ces.h>
#include <stdbool.h>
#include <time.h>

struct Url;
struct FtpFile {
    struct input_stream* is;
    time_t modtime;
};
struct FtpFile
openFTPStream(struct Url* pu);
Str loadFTPDir(struct Url* pu, wc_ces* charset, bool do_download);
void closeFTP(void);
void disconnectFTP(void);
