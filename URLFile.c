#include "URLFile.h"
#include "ftp.h"
#include <string.h>

void init_stream(struct URLFile* uf, int scheme, struct input_stream* stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->modtime = -1;
}

void UFhalfclose(struct URLFile* f)
{
    switch (f->scheme) {
    case SCM_FTP:
        closeFTP();
        break;
    default:
        is_close(f->stream);
        break;
    }
}

