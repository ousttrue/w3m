#pragma once
#include "text/Str.h"

struct Url;
struct URLFile;
extern union input_stream *openFTPStream(struct Url *pu, struct URLFile *uf);
extern Str loadFTPDir0(struct Url *pu);
#define loadFTPDir(pu, charset) loadFTPDir0(pu)
extern void closeFTP(void);
extern void disconnectFTP(void);
