#include "URLFile.h"
#include <string.h>

void init_stream(URLFile* uf, int scheme, union input_stream* stream)
{
    memset(uf, 0, sizeof(URLFile));
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
