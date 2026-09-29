#pragma once
#include "url.h"
#include "istream.h"

enum StreamEncoding {
    ENC_7BIT,
    ENC_BASE64,
    ENC_QUOTE,
    ENC_UUENCODE,
};

enum CompressionType {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

struct UrlStream {
    enum UrlScheme scheme;
    union input_stream* stream;
    enum StreamEncoding encoding;
    bool is_cgi;
    enum CompressionType compression;
    enum CompressionType content_encoding;
    const char* guess_type;
    const char* ext;
    time_t modtime;
    const char* ssl_certificate;
    const char* url;
};
typedef struct UrlStream URLFile;

typedef struct {
    char* referer;
    int flag;
} URLOption;

struct UrlStream init_stream(enum UrlScheme scheme, InputStream stream);
