#pragma once
#include "url.h"
#include "input_stream.h"
#include "compression.h"
#include <time.h>

struct UrlStream {
    enum UrlScheme scheme;
    struct input_stream* stream;
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

inline static void us_close(struct UrlStream* f)
{
    if (IS_close((f)->stream) == 0) {
        (f)->stream = NULL;
    }
}

struct URLOption {
    const char* referer;
    int flag;
};

struct UrlStream init_stream(enum UrlScheme scheme, struct input_stream* stream);

void UFhalfclose(URLFile* f);
