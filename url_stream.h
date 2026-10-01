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

#define StrUFgets(f) StrISgets((f)->stream)
#define StrmyUFgets(f) StrmyISgets((f)->stream)
#define UFgetc(f) ISgetc((f)->stream)
#define UFundogetc(f) ISundogetc((f)->stream)
#define UFclose(f)                   \
    if (ISclose((f)->stream) == 0) { \
        (f)->stream = NULL;          \
    }
#define UFfileno(f) ISfileno((f)->stream)

typedef struct {
    char* referer;
    int flag;
} URLOption;

struct UrlStream init_stream(enum UrlScheme scheme, struct input_stream* stream);

void UFhalfclose(URLFile* f);
