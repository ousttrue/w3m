#pragma once
#include "text/Str.h"

extern const char *ftppasswd;
extern bool ftppass_hostnamegen;

struct Url;
struct URLFile;
extern union input_stream *openFTPStream(struct Url *pu);
extern Str loadFTPDir0(struct Url *pu);
extern void closeFTP(void);
extern void disconnectFTP(void);
