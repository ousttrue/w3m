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

#define StrUFgets(f) IS_gets((f)->stream, false)
#define StrmyUFgets(f) IS_gets((f)->stream, true)
#define UFgetc(f) IS_getc((f)->stream)
#define UFundogetc(f) IS_ungetc((f)->stream)
#define UFclose(f)                   \
    if (IS_close((f)->stream) == 0) { \
        (f)->stream = NULL;          \
    }
#define UFfileno(f) IS_FD((f)->stream)

struct URLOption {
    const char* referer;
    int flag;
};

struct UrlStream init_stream(enum UrlScheme scheme, struct input_stream* stream);

void UFhalfclose(URLFile* f);
