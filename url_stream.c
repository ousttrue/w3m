#include "url_stream.h"
#include "proto.h"

struct UrlStream init_stream(enum UrlScheme scheme, struct input_stream* stream)
{
    struct UrlStream uf = {
        .scheme = scheme,
        .stream = stream,
        .encoding = ENC_7BIT,
        .is_cgi = false,
        .compression = CMP_NOCOMPRESS,
        .content_encoding = CMP_NOCOMPRESS,
        .guess_type = 0,
        .ext = 0,
        .modtime = -1,
        .ssl_certificate = 0,
        .url = 0,
    };
    return uf;
}

void UFhalfclose(URLFile* f)
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
