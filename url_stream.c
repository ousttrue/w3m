#include "url_stream.h"

struct UrlStream init_stream(enum UrlScheme scheme, InputStream stream)
{
    struct UrlStream uf = {
        .scheme = scheme,
        .stream = stream,
        .encoding = ENC_7BIT,
        .is_cgi = FALSE,
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
