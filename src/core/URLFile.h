#pragma once
#include "url_scheme.h"
#include "compression.h"
#include <time.h>

struct URLFile {
    enum UrlScheme scheme;
    char is_cgi;
    char encoding;
    union input_stream* stream;
    const char* ext;
    enum CompressionType compression;
    int content_encoding;
    const char* guess_type;
    const char* ssl_certificate;
    const char* url;
    time_t modtime;
};

int doFileSave(struct URLFile uf, const char* defstr, int current_content_length);
union input_stream;
void init_stream(struct URLFile* uf, int scheme, union input_stream* stream);
