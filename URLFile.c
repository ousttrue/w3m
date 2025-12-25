#include "URLFile.h"
#include "ftp.h"
#include "news.h"
#include <string.h>

void init_stream(struct URLFile* uf, int scheme, union input_stream* stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->encoding = ENC_7BIT;
    uf->is_cgi = FALSE;
    uf->compression = CMP_NOCOMPRESS;
    uf->content_encoding = CMP_NOCOMPRESS;
    uf->guess_type = NULL;
    uf->ext = NULL;
    uf->modtime = -1;
}

void UFhalfclose(struct URLFile* f)
{
    switch (f->scheme) {
    case SCM_FTP:
        closeFTP();
        break;
    case SCM_NEWS:
    case SCM_NNTP:
        closeNews();
        break;
    default:
        UFclose(f);
        break;
    }
}
